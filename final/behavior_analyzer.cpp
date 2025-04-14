#include "behavior_analyzer.h"
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>

// Define the window class name
const char* BehaviorAnalyzer::ANALYZER_WINDOW_CLASS = "BehaviorAnalyzerWindowClass";

// Helper to convert vkCode to a char if possible (extremely simplified)
static char VkToChar(DWORD vkCode)
{
    // Basic alpha-numeric assumption
    if ((vkCode >= 0x30 && vkCode <= 0x39) || (vkCode >= 0x41 && vkCode <= 0x5A)) {
        return static_cast<char>(vkCode);
    }
    return '\0';
}

// Window procedure for processing Raw Input messages
LRESULT CALLBACK BehaviorAnalyzer::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
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
                    std::string deviceId = analyzer.GetDeviceNameFromHandle(raw->header.hDevice);
                    
                    // Get the keycode
                    DWORD vkCode = raw->data.keyboard.VKey;
                    
                    // Process this keystroke
                    analyzer.ProcessKeystroke(deviceId, vkCode);
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
BehaviorAnalyzer& BehaviorAnalyzer::GetInstance()
{
    static BehaviorAnalyzer instance;
    return instance;
}

BehaviorAnalyzer::BehaviorAnalyzer()
    : messageWindow(nullptr), blockSuspiciousInput(false)
{
    // Initialize with common blacklisted words
    blacklistedWords.insert("cmd");
    blacklistedWords.insert("powershell");
}

BehaviorAnalyzer::~BehaviorAnalyzer()
{
    Stop();
}

void BehaviorAnalyzer::Start()
{

    // Register window class for Raw Input
    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = WindowProc;
    wcex.hInstance = GetModuleHandle(nullptr);
    wcex.lpszClassName = ANALYZER_WINDOW_CLASS;
    
    if (!RegisterClassEx(&wcex)) {
        std::cerr << "[BehaviorAnalyzer] Failed to register window class\n";
        return;
    }
    
    // Create a hidden window to receive Raw Input
    messageWindow = CreateWindow(
        ANALYZER_WINDOW_CLASS,
        "Behavior Analyzer Window",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );
    
    if (!messageWindow) {
        std::cerr << "[BehaviorAnalyzer] Failed to create message window\n";
        UnregisterClass(ANALYZER_WINDOW_CLASS, GetModuleHandle(nullptr));
        return;
    }
    
    // Register for Raw Input
    RegisterRawInput();
}

void BehaviorAnalyzer::Stop()
{
    if (messageWindow) {
        DestroyWindow(messageWindow);
        messageWindow = nullptr;
        UnregisterClass(ANALYZER_WINDOW_CLASS, GetModuleHandle(nullptr));
    }
}

void BehaviorAnalyzer::RegisterRawInput()
{
    // Register to receive input from all keyboard devices
    RAWINPUTDEVICE rid[1];
    rid[0].usUsagePage = 0x01;          // HID_USAGE_PAGE_GENERIC
    rid[0].usUsage = 0x06;              // HID_USAGE_GENERIC_KEYBOARD
    rid[0].dwFlags = RIDEV_INPUTSINK;   // Receive input even when not in foreground
    rid[0].hwndTarget = messageWindow;  // Window to receive input
    
    if (!RegisterRawInputDevices(rid, 1, sizeof(RAWINPUTDEVICE))) {
        DWORD error = GetLastError();
        std::cerr << "[BehaviorAnalyzer] Failed to register raw input devices. Error code: " << error << "\n";
    }
}

std::string BehaviorAnalyzer::GetDeviceNameFromHandle(HANDLE hDevice)
{
    // Get the device name size
    UINT nameSize = 0;
    GetRawInputDeviceInfo(hDevice, RIDI_DEVICENAME, nullptr, &nameSize);
    
    if (nameSize > 0) {
        // Allocate buffer for the name
        std::vector<char> deviceName(nameSize);
        
        // Get the device name
        if (GetRawInputDeviceInfo(hDevice, RIDI_DEVICENAME, deviceName.data(), &nameSize) > 0) {
            return std::string(deviceName.data());
        }
    }
    
    // Return a placeholder if device name cannot be retrieved
    std::stringstream ss;
    ss << "unknown-device-" << std::hex << std::setw(8) << std::setfill('0') << (uintptr_t)hDevice;
    return ss.str();
}

void BehaviorAnalyzer::ProcessKeystroke(const std::string& deviceId, DWORD vkCode)
{
    // Get current time to calculate interval
    auto now = std::chrono::steady_clock::now();
    
    // Create device data entry if it doesn't exist
    if (deviceDataMap.find(deviceId) == deviceDataMap.end()) {
        DeviceBehaviorData newData;
        newData.deviceId = deviceId;
        newData.deviceName = deviceId; // Could be updated later with a friendly name
        deviceDataMap[deviceId] = newData;
    }
    
    // Calculate time interval since last keystroke
    auto& deviceData = deviceDataMap[deviceId];
    int interval = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - deviceData.lastKeyPressTime).count();
    deviceData.lastKeyPressTime = now;

    // Temporarily log the keystroke for debugging
    logFile.open("sus.log", std::ios::out | std::ios::app);
    if (logFile.is_open()) {
        // Get current system time for logging
        auto system_now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(system_now);
        std::tm now_tm;
        localtime_s(&now_tm, &now_c);
        
        logFile << "[" << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S") << "] "
                << "Device: " << deviceId << ", Key: " << VkToChar(vkCode) << ", Interval: " << interval << "ms\n";
        logFile.close();
    }
    
    // Check for suspicious activity using the extracted method
    CheckForSuspiciousActivity(deviceId, interval);
    
    // Analyze the keystroke for blacklisted words
    AnalyzeKeystrokes(deviceId, vkCode);
}

void BehaviorAnalyzer::CheckForSuspiciousActivity(const std::string& deviceId, int interval)
{
    auto& deviceData = deviceDataMap[deviceId];
    
    // Add the interval to our tracking
    deviceData.keyPressIntervals.push_back(interval);
    
    // Keep a reasonable history size (last 20 keystrokes)
    if (deviceData.keyPressIntervals.size() > 20) {
        deviceData.keyPressIntervals.erase(deviceData.keyPressIntervals.begin());
    }
    
    // Multiple suspicious patterns we can detect:
    
    // 1. Too many fast keystrokes in a row (potential automated typing)
    int fastCount = 0;
    for (int i : deviceData.keyPressIntervals) {
        if (i < 30) { // Less than 30ms between keys
            fastCount++;
        }
    }
    
    // 2. Extremely consistent typing speed (inhuman regularity)
    bool consistentTiming = false;
    if (deviceData.keyPressIntervals.size() >= 5) {
        int similarIntervals = 0;
        int lastInterval = deviceData.keyPressIntervals[0];
        
        for (size_t i = 1; i < deviceData.keyPressIntervals.size(); i++) {
            int current = deviceData.keyPressIntervals[i];
            // Check if intervals are within 5ms of each other
            if (abs(current - lastInterval) <= 5) {
                similarIntervals++;
            }
            lastInterval = current;
        }
        
        consistentTiming = (similarIntervals >= 5); // 5+ similar intervals in a row
    }
    
    // If any suspicious pattern is detected
    if (fastCount >= 8 || consistentTiming) {
        // Log suspicious activity
        std::stringstream activityLog;
        activityLog << "[BehaviorAnalyzer] Suspicious keyboard activity detected from device: " << deviceId << "\n";
        activityLog << "  - Fast keystrokes: " << fastCount << " (threshold: 8)\n";
        activityLog << "  - Consistent timing pattern: " << (consistentTiming ? "Yes" : "No") << "\n";
        
        std::cerr << activityLog.str();
        
        // We could also log to a file
        logFile.open("sus.log", std::ios::out | std::ios::app);
        if (logFile.is_open()) {
            // Add timestamp
            auto now = std::chrono::system_clock::now();
            std::time_t now_c = std::chrono::system_clock::to_time_t(now);
            std::tm now_tm;
            localtime_s(&now_tm, &now_c);
            
            logFile << "[" << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S") << "] ";
            logFile << activityLog.str();
            logFile.close();
        }
        
        // Notify listeners
        NotifyListeners(AnalyzerEvent::SUSPICIOUS_TYPING_DETECTED, deviceId);
    }
}

void BehaviorAnalyzer::AddBlacklistedWord(const std::string& word)
{
    blacklistedWords.insert(word);
}

void BehaviorAnalyzer::AnalyzeKeystrokes(const std::string& deviceId, DWORD vkCode)
{
    // Get device data
    auto& deviceData = deviceDataMap[deviceId];
    
    char c = VkToChar(vkCode);
    // If non-alphanumeric, treat as potential delimiter
    if (c == '\0') {
        if (!deviceData.currentInput.empty()) {
            // Once we see a delimiter, check the entire chunk
            if (ContainsBlacklistedWord(deviceData.currentInput)) {
                std::cerr << "[BehaviorAnalyzer] Blacklisted word detected in: "
                          << deviceData.currentInput << " from device: " << deviceId << "\n";
                
                // Notify listeners of the blacklisted word detection
                NotifyListeners(AnalyzerEvent::BLACKLISTED_WORD_DETECTED, deviceId);
            }
            deviceData.currentInput.clear();
        }
        return;
    }

    // Append to the current input buffer
    deviceData.currentInput.push_back(c);

    // Optionally do partial checks for blacklisted words
    if (ContainsBlacklistedWord(deviceData.currentInput)) {
        std::cerr << "[BehaviorAnalyzer] Blacklisted word partially matched in: "
                  << deviceData.currentInput << " from device: " << deviceId << "\n";
        
        // Notify listeners of the partial match
        NotifyListeners(AnalyzerEvent::BLACKLISTED_WORD_PARTIAL_MATCH, deviceId);
    }
}

bool BehaviorAnalyzer::ContainsBlacklistedWord(const std::string& input)
{
    for (const auto& word : blacklistedWords) {
        if (input.find(word) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void BehaviorAnalyzer::RegisterListener(IAnalyzerListener* listener)
{
    if (listener) {
        listeners.push_back(listener);
    }
}

void BehaviorAnalyzer::UnregisterListener(IAnalyzerListener* listener)
{
    listeners.erase(
        std::remove(listeners.begin(), listeners.end(), listener),
        listeners.end()
    );
}

void BehaviorAnalyzer::NotifyListeners(AnalyzerEvent event, const std::string& deviceId)
{
    for (auto listener : listeners) {
        listener->OnAnalyzerEvent(event, deviceId);
    }
}

void BehaviorAnalyzer::SetBlockSuspiciousInput(bool block)
{
    blockSuspiciousInput = block;
}

bool BehaviorAnalyzer::GetBlockSuspiciousInput() const
{
    return blockSuspiciousInput;
}