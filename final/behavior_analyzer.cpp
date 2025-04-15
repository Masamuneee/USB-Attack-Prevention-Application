#include "behavior_analyzer.h"

// Define the window class name
const char* BehaviorAnalyzer::kAnalyzerWindowClass = "BehaviorAnalyzerWindowClass";

// Define Raw Input constants if they aren't available
#ifndef WM_INPUT
#define WM_INPUT 0x00FF
#endif

#ifndef RIM_TYPEKEYBOARD
#define RIM_TYPEKEYBOARD 1
#endif

#ifndef RIDEV_INPUTSINK
#define RIDEV_INPUTSINK 0x00000100
#endif

#ifndef RIDI_DEVICENAME
#define RIDI_DEVICENAME 0x20000007
#endif

// Helper to convert vkCode to a char if possible
static char VkToChar(DWORD vk_code) {
  // Basic alpha-numeric assumption
  if ((vk_code >= 0x30 && vk_code <= 0x39) || (vk_code >= 0x41 && vk_code <= 0x5A)) {
    return static_cast<char>(vk_code);
  }
  return '\0';
}

// Window procedure for processing Raw Input messages
LRESULT CALLBACK BehaviorAnalyzer::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  BehaviorAnalyzer& analyzer = BehaviorAnalyzer::GetInstance();
  
  switch (uMsg) {
    case WM_INPUT: {
      UINT dwSize = 0;
      
      // First get the size of the input data
      GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
      
      if (dwSize > 0) {
        // Allocate buffer for the input data
        LPBYTE lpb = new BYTE[dwSize];
        if (lpb == NULL) {
          return 0;
        }
        
        // Now get the data
        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
          delete[] lpb;
          return 0;
        }
        
        // Process the raw input data
        RAWINPUT* raw = (RAWINPUT*)lpb;
        
        // Check if it's a keyboard event
        if (raw->header.dwType == RIM_TYPEKEYBOARD && 
            (raw->data.keyboard.Message == WM_KEYDOWN || raw->data.keyboard.Message == WM_SYSKEYDOWN)) {
          
          // Get the device identifier
          std::string device_id = analyzer.GetDeviceNameFromHandle(raw->header.hDevice);
          
          // Get the keycode
          DWORD vk_code = raw->data.keyboard.VKey;
          
          // Process this keystroke
          analyzer.ProcessKeystroke(device_id, vk_code);
        }
        
        delete[] lpb;
      }
      return 0;
    }
    
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Singleton implementation
BehaviorAnalyzer& BehaviorAnalyzer::GetInstance() {
  static BehaviorAnalyzer instance;
  return instance;
}

BehaviorAnalyzer::BehaviorAnalyzer()
    : message_window_(nullptr), block_suspicious_input_(false) {
  // Initialize with common blacklisted words
  blacklisted_words_.insert("cmd");
  blacklisted_words_.insert("powershell");
}

BehaviorAnalyzer::~BehaviorAnalyzer() {
  Stop();
}

void BehaviorAnalyzer::Start() {
  // Register window class for Raw Input
  WNDCLASSEX wcex = {0};
  wcex.cbSize = sizeof(WNDCLASSEX);
  wcex.lpfnWndProc = WindowProc;
  wcex.hInstance = GetModuleHandle(nullptr);
  wcex.lpszClassName = kAnalyzerWindowClass;
  
  if (!RegisterClassEx(&wcex)) {
    std::cerr << "[BehaviorAnalyzer] Failed to register window class\n";
    return;
  }
  
  // Create a hidden window to receive Raw Input
  message_window_ = CreateWindow(
      kAnalyzerWindowClass,
      "Behavior Analyzer Window",
      0, 0, 0, 0, 0,
      HWND_MESSAGE, nullptr,
      GetModuleHandle(nullptr),
      nullptr
  );
  
  if (!message_window_) {
    std::cerr << "[BehaviorAnalyzer] Failed to create message window\n";
    UnregisterClass(kAnalyzerWindowClass, GetModuleHandle(nullptr));
    return;
  }
  
  // Register for Raw Input
  RegisterRawInput();
}

void BehaviorAnalyzer::Stop() {
  if (message_window_) {
    DestroyWindow(message_window_);
    message_window_ = nullptr;
    UnregisterClass(kAnalyzerWindowClass, GetModuleHandle(nullptr));
  }
}

void BehaviorAnalyzer::RegisterRawInput() {
  // Register to receive input from all keyboard devices
  RAWINPUTDEVICE rid[1];
  rid[0].usUsagePage = 0x01;          // HID_USAGE_PAGE_GENERIC
  rid[0].usUsage = 0x06;              // HID_USAGE_GENERIC_KEYBOARD
  rid[0].dwFlags = RIDEV_INPUTSINK;   // Receive input even when not in foreground
  rid[0].hwndTarget = message_window_;  // Window to receive input
  
  if (!RegisterRawInputDevices(rid, 1, sizeof(RAWINPUTDEVICE))) {
    DWORD error = GetLastError();
    std::cerr << "[BehaviorAnalyzer] Failed to register raw input devices. Error code: " << error << "\n";
  }
}

std::string BehaviorAnalyzer::GetDeviceNameFromHandle(HANDLE h_device) {
  // Get the device name size
  UINT name_size = 0;
  GetRawInputDeviceInfo(h_device, RIDI_DEVICENAME, nullptr, &name_size);
  
  if (name_size > 0) {
    // Allocate buffer for the name
    std::vector<char> device_name(name_size);
    
    // Get the device name
    if (GetRawInputDeviceInfo(h_device, RIDI_DEVICENAME, device_name.data(), &name_size) > 0) {
      return std::string(device_name.data());
    }
  }
  
  // Return a placeholder if device name cannot be retrieved
  std::stringstream ss;
  ss << "unknown-device-" << std::hex << std::setw(8) << std::setfill('0') << (uintptr_t)h_device;
  return ss.str();
}

void BehaviorAnalyzer::ProcessKeystroke(const std::string& device_id, DWORD vk_code) {
  // Get current time to calculate interval
  auto now = std::chrono::steady_clock::now();
  
  // Create device data entry if it doesn't exist
  if (device_data_map_.find(device_id) == device_data_map_.end()) {
    DeviceBehaviorData new_data;
    new_data.device_id = device_id;
    new_data.device_name = device_id; // Could be updated later with a friendly name
    device_data_map_[device_id] = new_data;
  }
  
  // Calculate time interval since last keystroke
  auto& device_data = device_data_map_[device_id];
  int interval = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - device_data.last_key_press_time).count();
  device_data.last_key_press_time = now;

  // Temporarily log the keystroke for debugging
  log_file_.open("sus.log", std::ios::out | std::ios::app);
  if (log_file_.is_open()) {
    // Get current system time for logging
    auto system_now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(system_now);
    std::tm now_tm = {};
    
    #ifdef _MSC_VER
      localtime_s(&now_tm, &now_c);
    #else
      // Use standard localtime for non-MSVC compilers
      std::tm* temp_tm = localtime(&now_c);
      if (temp_tm) {
        now_tm = *temp_tm;
      }
    #endif
    
    log_file_ << "[" << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S") << "] "
              << "Device: " << device_id << ", Key: " << VkToChar(vk_code) << ", Interval: " << interval << "ms\n";
    log_file_.close();
  }
  
  // Check for suspicious activity using the extracted method
  CheckForSuspiciousActivity(device_id, interval);
  
  // Analyze the keystroke for blacklisted words
  AnalyzeKeystrokes(device_id, vk_code);
}

void BehaviorAnalyzer::CheckForSuspiciousActivity(const std::string& device_id, int interval) {
  auto& device_data = device_data_map_[device_id];
  
  // Add the interval to our tracking
  device_data.key_press_intervals.push_back(interval);
  
  // Keep a reasonable history size (last 20 keystrokes)
  if (device_data.key_press_intervals.size() > 20) {
    device_data.key_press_intervals.erase(device_data.key_press_intervals.begin());
  }
  
  // Multiple suspicious patterns we can detect:
  
  // 1. Too many fast keystrokes in a row (potential automated typing)
  int fast_count = 0;
  for (int i : device_data.key_press_intervals) {
    if (i < 30) { // Less than 30ms between keys
      fast_count++;
    }
  }
  
  // 2. Extremely consistent typing speed (inhuman regularity)
  bool consistent_timing = false;
  if (device_data.key_press_intervals.size() >= 5) {
    int similar_intervals = 0;
    int last_interval = device_data.key_press_intervals[0];
    
    for (size_t i = 1; i < device_data.key_press_intervals.size(); i++) {
      int current = device_data.key_press_intervals[i];
      // Check if intervals are within 5ms of each other
      if (abs(current - last_interval) <= 5) {
        similar_intervals++;
      }
      last_interval = current;
    }
    
    consistent_timing = (similar_intervals >= 5); // 5+ similar intervals in a row
  }
  
  // If any suspicious pattern is detected
  if (fast_count >= 8 || consistent_timing) {
    // Log suspicious activity
    std::stringstream activity_log;
    activity_log << "[BehaviorAnalyzer] Suspicious keyboard activity detected from device: " << device_id << "\n";
    activity_log << "  - Fast keystrokes: " << fast_count << " (threshold: 8)\n";
    activity_log << "  - Consistent timing pattern: " << (consistent_timing ? "Yes" : "No") << "\n";
    
    std::cerr << activity_log.str();
    
    // We could also log to a file
    log_file_.open("sus.log", std::ios::out | std::ios::app);
    if (log_file_.is_open()) {
      // Add timestamp
      auto now = std::chrono::system_clock::now();
      std::time_t now_c = std::chrono::system_clock::to_time_t(now);
      std::tm now_tm = {};
      
      #ifdef _MSC_VER
        localtime_s(&now_tm, &now_c);
      #else
        // Use standard localtime for non-MSVC compilers
        std::tm* temp_tm = localtime(&now_c);
        if (temp_tm) {
          now_tm = *temp_tm;
        }
      #endif
      
      log_file_ << "[" << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S") << "] ";
      log_file_ << activity_log.str();
      log_file_.close();
    }
    
    // Notify listeners
    NotifyListeners(AnalyzerEvent::kSuspiciousTypingDetected, device_id);
  }
}

void BehaviorAnalyzer::AddBlacklistedWord(const std::string& word) {
  blacklisted_words_.insert(word);
}

void BehaviorAnalyzer::AnalyzeKeystrokes(const std::string& device_id, DWORD vk_code) {
  // Get device data
  auto& device_data = device_data_map_[device_id];
  
  char c = VkToChar(vk_code);
  // If non-alphanumeric, treat as potential delimiter
  if (c == '\0') {
    if (!device_data.current_input.empty()) {
      // Once we see a delimiter, check the entire chunk
      if (ContainsBlacklistedWord(device_data.current_input)) {
        std::cerr << "[BehaviorAnalyzer] Blacklisted word detected in: "
                  << device_data.current_input << " from device: " << device_id << "\n";
        
        // Notify listeners of the blacklisted word detection
        NotifyListeners(AnalyzerEvent::kBlacklistedWordDetected, device_id);
      }
      device_data.current_input.clear();
    }
    return;
  }

  // Append to the current input buffer
  device_data.current_input.push_back(c);

  // Optionally do partial checks for blacklisted words
  if (ContainsBlacklistedWord(device_data.current_input)) {
    std::cerr << "[BehaviorAnalyzer] Blacklisted word partially matched in: "
              << device_data.current_input << " from device: " << device_id << "\n";
    
    // Notify listeners of the partial match
    NotifyListeners(AnalyzerEvent::kBlacklistedWordPartialMatch, device_id);
  }
}

bool BehaviorAnalyzer::ContainsBlacklistedWord(const std::string& input) {
  for (const auto& word : blacklisted_words_) {
    if (input.find(word) != std::string::npos) {
      return true;
    }
  }
  return false;
}

void BehaviorAnalyzer::RegisterListener(IAnalyzerListener* listener) {
  if (listener) {
    listeners_.push_back(listener);
  }
}

void BehaviorAnalyzer::UnregisterListener(IAnalyzerListener* listener) {
  listeners_.erase(
      std::remove(listeners_.begin(), listeners_.end(), listener),
      listeners_.end()
  );
}

void BehaviorAnalyzer::NotifyListeners(AnalyzerEvent event, const std::string& device_id) {
  for (auto listener : listeners_) {
    listener->OnAnalyzerEvent(event, device_id);
  }
}

void BehaviorAnalyzer::SetBlockSuspiciousInput(bool block) {
  block_suspicious_input_ = block;
}

bool BehaviorAnalyzer::GetBlockSuspiciousInput() const {
  return block_suspicious_input_;
}