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
// Helper to convert vkCode to a char if possible (improved version)
static char VkToChar(DWORD vk_code) {
    // Basic alpha-numeric conversion
    if (vk_code >= 0x30 && vk_code <= 0x39) {
        // Numbers 0-9
        return static_cast<char>(vk_code);
    } else if (vk_code >= 0x41 && vk_code <= 0x5A) {
        // Letters A-Z
        return static_cast<char>(vk_code);
    } else if (vk_code == VK_SPACE) {
        // Space character
        return ' ';
    } else if (vk_code == VK_OEM_PERIOD) {
        return '.';
    } else if (vk_code == VK_OEM_COMMA) {
        return ',';
    } else if (vk_code == VK_OEM_MINUS) {
        return '-';
    } else if (vk_code == VK_OEM_PLUS) {
        return '+';
    } else if (vk_code == VK_OEM_1) {
        return ';';  // Semicolon
    } else if (vk_code == VK_OEM_2) {
        return '/';  // Forward slash
    } else if (vk_code == VK_OEM_3) {
        return '`';  // Backtick
    } else if (vk_code == VK_OEM_4) {
        return '[';  // Left bracket
    } else if (vk_code == VK_OEM_5) {
        return '\\'; // Backslash
    } else if (vk_code == VK_OEM_6) {
        return ']';  // Right bracket
    } else if (vk_code == VK_OEM_7) {
        return '\''; // Single quote
    }
    
    // Not a character we want to track
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

// Set block_suspicious_input_ to true if you want to eject malicious devices
BehaviorAnalyzer::BehaviorAnalyzer() : message_window_(nullptr), block_suspicious_input_(false) {
    // Initialize with common blacklisted words
    blacklisted_words_.insert("CMD");
    blacklisted_words_.insert("POWERSHELL");
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

    // Check if this is a repeated key (same as last keypress)
    if (vk_code == device_data.last_key_code) {
        device_data.repeated_key_count++;
    } 
    else {
        device_data.repeated_key_count = 0;
        device_data.last_key_code = vk_code;
    }

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
                << "Device: " << device_id << ", Key: " << VkToChar(vk_code) 
                << ", Interval: " << interval << "ms"
                << ", Repeated: " << device_data.repeated_key_count << "\n";
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
    
    // Keep a reasonable history size (last 10 keystrokes)
    if (device_data.key_press_intervals.size() > 10) {
        device_data.key_press_intervals.erase(device_data.key_press_intervals.begin());
    }

    // Skip suspicious activity detection if we have a key being held down
    if (device_data.repeated_key_count >= 3) {
        return;
    }
    
    // Multiple suspicious patterns we can detect:
    
    // 1. Too many fast keystrokes in a row (potential automated typing)
    int fast_count = 0;
    for (int i : device_data.key_press_intervals) {
        if (i < 12) { // Less than 12ms between keys
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

        // If blocking is enabled, clear the input buffer
        if (block_suspicious_input_) {
            device_data.current_input.clear();
            // Get the DeviceAuthenticator instance to handle the block
            DeviceAuthenticator& authenticator = DeviceAuthenticator::GetInstance();
            authenticator.EjectDeviceById(device_id); // Eject the device
            std::cerr << "[BehaviorAnalyzer] Input blocked due to suspicious activity\n";
        }
        
        // Notify listeners
        NotifyListeners(AnalyzerEvent::kSuspiciousTypingDetected, device_id);
    }
}

void BehaviorAnalyzer::AddBlacklistedWord(const std::string& word) {
    std::string uppercase_word = word;
    std::transform(uppercase_word.begin(), uppercase_word.end(), uppercase_word.begin(), [](unsigned char c) {
            return std::toupper(c); 
        }
    );
    blacklisted_words_.insert(uppercase_word);
}

void BehaviorAnalyzer::AnalyzeKeystrokes(const std::string& device_id, DWORD vk_code) {
    // Get device data
    auto& device_data = device_data_map_[device_id];
    // Open log file for writing
    log_file_.open("sus.log", std::ios::out | std::ios::app);
    
    // Handle backspace key specifically
    if (vk_code == VK_BACK) {
        if (!device_data.current_input.empty()) {
            // Remove the last character from input buffer to simulate backspace
            device_data.current_input.pop_back();
            
            if (log_file_.is_open()) {
                log_file_ << "[Info] Backspace detected, current input: " << device_data.current_input << "\n";
            }
        }
        if (log_file_.is_open()) {
            log_file_.close();
        }
        return;
    }
    
    // Get character representation if possible
    char c = VkToChar(vk_code);

    // Append to the current input buffer
    device_data.current_input.push_back(c);
    // if (log_file_.is_open()) {
    //     log_file_ << "[Info] Current input: " << device_data.current_input << "\n";
    // }
    
    // Keep a reasonable size for the current input buffer
    if (device_data.current_input.size() > 100) {
        device_data.current_input.erase(device_data.current_input.begin(), device_data.current_input.begin() + 50);
    }
    
    // Implement a stronger detection by creating a normalized version (with non-alphanumeric characters removed)
    std::string normalized_check = device_data.current_input;
    normalized_check.erase(
        std::remove_if(normalized_check.begin(), normalized_check.end(), 
                       [](char c) { return !std::isalnum(c); }),
        normalized_check.end());
    
    // Check the normalized string for blacklisted words
    if (ContainsBlacklistedWord(normalized_check)) {
        if (log_file_.is_open()) {
            log_file_ << "[BehaviorAnalyzer] Blacklisted word detected in normalized input: "
                    << normalized_check << " from device: " << device_id << "\n";
            log_file_ << "[BehaviorAnalyzer] Original input was: " << device_data.current_input << "\n";
        }
        
        // Notify listeners of the blacklisted word detection
        NotifyListeners(AnalyzerEvent::kBlacklistedWordDetected, device_id);
        
        // If blocking is enabled, clear the input buffer
        if (block_suspicious_input_) {
            device_data.current_input.clear();
            // Get the DeviceAuthenticator instance to handle the block
            DeviceAuthenticator& authenticator = DeviceAuthenticator::GetInstance();
            authenticator.EjectDeviceById(device_id); // Eject the device
            std::cerr << "[BehaviorAnalyzer] Input blocked due to blacklisted word match in normalized text\n";
        }
    }
    
    // Close the log file
    if (log_file_.is_open()) {
        log_file_.close();
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