#include "key_logger.h"

// Window class name for raw input
const char* RAW_INPUT_CLASS = "USBMonitorRawInputClass";
// Window message for raw input processing
// const UINT WM_INPUT_DEVICE_CHANGE = 0x00FE;

// Utility function to get current date/time in string form.
static std::string GetCurrentDateTime()
{
    auto now = std::chrono::system_clock::now();
    std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    
    std::tm nowTm;
    localtime_s(&nowTm, &nowTime); // Use safer localtime_s instead of localtime
    
    std::ostringstream oss;
    oss << std::put_time(&nowTm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// Helper to translate vkCode to a human-readable string
static std::string VkCodeToString(DWORD vkCode)
{
    // Basic alphanumeric keys
    if ((vkCode >= 0x30 && vkCode <= 0x39) || (vkCode >= 0x41 && vkCode <= 0x5A)) {
        char c = static_cast<char>(vkCode);
        return std::string(1, c);
    }

    // For other keys, we might do a switch-case
    switch (vkCode) {
        case VK_RETURN:     return "Enter";
        case VK_BACK:       return "Backspace";
        case VK_TAB:        return "Tab";
        case VK_SPACE:      return "Space";
        case VK_ESCAPE:     return "Escape";
        case VK_SHIFT:      return "Shift";
        case VK_CONTROL:    return "Control";
        case VK_MENU:       return "Alt";
        case VK_CAPITAL:    return "CapsLock";
        case VK_LEFT:       return "LeftArrow";
        case VK_RIGHT:      return "RightArrow";
        case VK_UP:         return "UpArrow";
        case VK_DOWN:       return "DownArrow";
        case VK_DELETE:     return "Delete";
        case VK_HOME:       return "Home";
        case VK_END:        return "End";
        case VK_PRIOR:      return "PageUp";
        case VK_NEXT:       return "PageDown";
        case VK_F1:         return "F1";
        case VK_F2:         return "F2";
        case VK_F3:         return "F3";
        case VK_F4:         return "F4";
        case VK_F5:         return "F5";
        case VK_F6:         return "F6";
        case VK_F7:         return "F7";
        case VK_F8:         return "F8";
        case VK_F9:         return "F9";
        case VK_F10:        return "F10";
        case VK_F11:        return "F11";
        case VK_F12:        return "F12";
        default:            break; // fall through
    }

    // Fallback: try using MapVirtualKey / GetKeyNameText
    UINT scanCode = MapVirtualKey(vkCode, MAPVK_VK_TO_VSC);
    char name[128];
    if (GetKeyNameTextA(scanCode << 16, name, sizeof(name)) > 0) {
        return std::string(name);
    }

    // If unknown, just return numeric code
    std::ostringstream oss;
    oss << "Unknown[" << vkCode << "]";
    return oss.str();
}

// Window procedure for raw input window
LRESULT CALLBACK KeyLogger::RawInputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Get instance from window property
    KeyLogger* logger = reinterpret_cast<KeyLogger*>(GetProp(hwnd, "KeyLoggerInstance"));
    
    switch (msg) {
        case WM_INPUT: {
            if (logger) {
                logger->ProcessRawInput((HRAWINPUT)lParam);
            }
            break;
        }
        case WM_INPUT_DEVICE_CHANGE: {
            // Handle new device or device removal
            if (wParam == GIDC_ARRIVAL) {
                // New device connected
                HANDLE deviceHandle = (HANDLE)lParam;
                if (logger) {
                    std::string deviceName = logger->GetDeviceName(deviceHandle);
                    logger->deviceMap[deviceHandle] = KeyboardDeviceInfo(deviceName, deviceName);
                }
            } else if (wParam == GIDC_REMOVAL) {
                // Device removed
                HANDLE deviceHandle = (HANDLE)lParam;
                if (logger) {
                    logger->deviceMap.erase(deviceHandle);
                }
            }
            break;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Static callback function for the hook
LRESULT CALLBACK KeyLogger::KeyStrokeLogger(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        auto* kbdStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        DWORD vkCode = kbdStruct->vkCode;

        // Access the singleton instance
        static KeyLogger& logger = KeyLogger::GetInstance();
        
        // Get the device that generated this keystroke
        HANDLE deviceHandle = logger.GetDeviceHandleFromVirtualKey(vkCode);
        std::string deviceName = "Unknown Device";
        
        if (deviceHandle != NULL && logger.deviceMap.find(deviceHandle) != logger.deviceMap.end()) {
            deviceName = logger.deviceMap[deviceHandle].deviceName;
            
            // Calculate interval since last key press for this device
            auto now = std::chrono::steady_clock::now();
            auto& deviceInfo = logger.deviceMap[deviceHandle];
            int interval = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
                now - deviceInfo.lastKeyPressTime).count();
            deviceInfo.lastKeyPressTime = now;
            
            // Log keystroke with device info
            logger.LogKeyStroke(vkCode, interval, deviceName);
            logger.CheckForSuspiciousActivity(interval, deviceInfo.deviceId);
        } else {
            // Fallback to general logging if device can't be identified
            auto now = std::chrono::steady_clock::now();
            int interval = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
                now - logger.lastKeyPressTime).count();
            logger.lastKeyPressTime = now;
            
            logger.LogKeyStroke(vkCode, interval, deviceName);
            logger.CheckForSuspiciousActivity(interval, "Unknown");
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

// Constructor
KeyLogger::KeyLogger()
    : keyboardHook(nullptr), rawInputWindow(nullptr), lastActiveDevice(NULL)
{
}

// Destructor
KeyLogger::~KeyLogger()
{
    Stop();
}

void KeyLogger::Start()
{
    // Open the log file in append mode
    logFile.open("keys.log", std::ios::out | std::ios::app);
    if (!logFile.is_open()) {
        MessageBoxA(nullptr, "Failed to open keys.log!", "KeyLogger Error", MB_ICONERROR);
    }

    lastKeyPressTime = std::chrono::steady_clock::now();
    
    // Register raw input to identify keyboard devices
    RegisterRawInput();

    // Install the hook
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyStrokeLogger, GetModuleHandle(nullptr), 0);
    if (!keyboardHook) {
        MessageBoxA(nullptr, "Failed to install low-level keyboard hook!", "KeyLogger Error", MB_ICONERROR);
    }
}

void KeyLogger::Stop()
{
    if (keyboardHook) {
        UnhookWindowsHookEx(keyboardHook);
        keyboardHook = nullptr;
    }

    if (logFile.is_open()) {
        logFile.close();
    }
    
    // Clean up raw input window
    if (rawInputWindow) {
        DestroyWindow(rawInputWindow);
        UnregisterClass(RAW_INPUT_CLASS, GetModuleHandle(nullptr));
        rawInputWindow = nullptr;
    }
}

void KeyLogger::RegisterRawInput()
{
    // Register window class for raw input
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = RawInputWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = RAW_INPUT_CLASS;
    
    if (!RegisterClassEx(&wc)) {
        MessageBoxA(nullptr, "Failed to register raw input window class!", "KeyLogger Error", MB_ICONERROR);
        return;
    }
    
    // Create invisible window to receive raw input
    rawInputWindow = CreateWindow(
        RAW_INPUT_CLASS, "USB Monitor Raw Input Window",
        WS_OVERLAPPEDWINDOW, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    if (!rawInputWindow) {
        MessageBoxA(nullptr, "Failed to create raw input window!", "KeyLogger Error", MB_ICONERROR);
        return;
    }
    
    // Store this instance pointer with the window
    SetProp(rawInputWindow, "KeyLoggerInstance", reinterpret_cast<HANDLE>(this));
    
    // Register to receive raw input from keyboards
    RAWINPUTDEVICE rid[1];
    rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
    rid[0].usUsage = HID_USAGE_GENERIC_KEYBOARD;
    rid[0].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
    rid[0].hwndTarget = rawInputWindow;
    
    if (!RegisterRawInputDevices(rid, 1, sizeof(RAWINPUTDEVICE))) {
        MessageBoxA(nullptr, "Failed to register for raw input devices!", "KeyLogger Error", MB_ICONERROR);
    }
    
    // Enumerate existing keyboard devices
    UINT numDevices = 0;
    GetRawInputDeviceList(nullptr, &numDevices, sizeof(RAWINPUTDEVICELIST));
    
    if (numDevices > 0) {
        std::vector<RAWINPUTDEVICELIST> deviceList(numDevices);
        if (GetRawInputDeviceList(&deviceList[0], &numDevices, sizeof(RAWINPUTDEVICELIST)) != (UINT)-1) {
            for (UINT i = 0; i < numDevices; i++) {
                if (deviceList[i].dwType == RIM_TYPEKEYBOARD) {
                    std::string deviceName = GetDeviceName(deviceList[i].hDevice);
                    deviceMap[deviceList[i].hDevice] = KeyboardDeviceInfo(deviceName, deviceName);
                }
            }
        }
    }
}

void KeyLogger::ProcessRawInput(HRAWINPUT hRawInput)
{
    UINT dataSize = 0;
    GetRawInputData(hRawInput, RID_INPUT, nullptr, &dataSize, sizeof(RAWINPUTHEADER));
    
    if (dataSize > 0) {
        std::vector<BYTE> buffer(dataSize);
        if (GetRawInputData(hRawInput, RID_INPUT, buffer.data(), &dataSize, sizeof(RAWINPUTHEADER)) == dataSize) {
            RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buffer.data());
            
            if (raw->header.dwType == RIM_TYPEKEYBOARD) {
                // Store the handle of the last active keyboard device
                lastActiveDevice = raw->header.hDevice;
                
                // Make sure this device is in our map
                if (deviceMap.find(raw->header.hDevice) == deviceMap.end()) {
                    std::string deviceName = GetDeviceName(raw->header.hDevice);
                    deviceMap[raw->header.hDevice] = KeyboardDeviceInfo(deviceName, deviceName);
                }
            }
        }
    }
}

std::string KeyLogger::GetDeviceName(HANDLE deviceHandle)
{
    UINT bufferSize = 0;
    GetRawInputDeviceInfo(deviceHandle, RIDI_DEVICENAME, nullptr, &bufferSize);
    
    if (bufferSize > 0) {
        std::vector<char> buffer(bufferSize);
        if (GetRawInputDeviceInfo(deviceHandle, RIDI_DEVICENAME, buffer.data(), &bufferSize) > 0) {
            return std::string(buffer.data());
        }
    }
    
    return "Unknown Device";
}

HANDLE KeyLogger::GetDeviceHandleFromVirtualKey(DWORD vkCode)
{
    // Since we can't directly map a virtual key to a device,
    // we use the last active device from raw input as our best guess
    return lastActiveDevice;
}

void KeyLogger::LogKeyStroke(DWORD vkCode, int interval, const std::string& deviceName)
{
    if (!logFile.is_open()) {
        return;
    }
    std::string keyName = VkCodeToString(vkCode);
    logFile << "[" << GetCurrentDateTime() << "] "
            << "Device: " << deviceName << " - "
            << "Key Pressed: " << keyName
            << " (" << vkCode << ") "
            << "- Interval: " << interval << "ms\n";
    logFile.flush();
}

void KeyLogger::CheckForSuspiciousActivity(int interval, const std::string& deviceId)
{
    // First check if we have a device-specific entry
    if (!deviceId.empty() && deviceId != "Unknown") {
        // Find the device in our map to access its intervals
        for (auto& pair : deviceMap) {
            if (pair.second.deviceId == deviceId) {
                auto& intervals = pair.second.recentIntervals;
                intervals.push_back(interval);
                if (intervals.size() > 10) {
                    intervals.erase(intervals.begin());
                }
                
                int suspiciousCount = 0;
                for (int i : intervals) {
                    if (i < 30) {
                        suspiciousCount++;
                    }
                }
                
                if (suspiciousCount >= 5) {
                    // Log warning with device info
                    if (logFile.is_open()) {
                        logFile << "[" << GetCurrentDateTime() << "] ALERT: Potential Keystroke Injection (Too Fast) from device: "
                                << pair.second.deviceName << "!\n";
                        logFile.flush();
                    }
                }
                return;
            }
        }
    }
    
    // Fallback to general monitoring if device not found
    recentIntervals.push_back(interval);
    if (recentIntervals.size() > 10) {
        recentIntervals.erase(recentIntervals.begin());
    }

    int suspiciousCount = 0;
    for (int i : recentIntervals) {
        if (i < 30) {
            suspiciousCount++;
        }
    }

    if (suspiciousCount >= 5) {
        // Log warning
        if (logFile.is_open()) {
            logFile << "[" << GetCurrentDateTime() << "] ALERT: Potential Keystroke Injection (Too Fast)!\n";
            logFile.flush();
        }
    }
}

// Singleton implementation
KeyLogger& KeyLogger::GetInstance()
{
    static KeyLogger instance;
    return instance;
}