// Compile with:
//   g++ hook.cpp -o hook.exe -mwindows
//   g++ hook.cpp -o hook.exe -mconsole

/*
 * KeyStroke Logger and Injection Attack Detector
 *
 * This program installs a low-level keyboard hook to log key presses and detect
 * potential keystroke injection attacks based on the timing between keystrokes.
 * It logs key events with timestamps and intervals to a log file and alerts the
 * user if suspicious timing patterns are detected.
 */

#include <windows.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <vector>
#include <sstream>

// Constants
const int kSuspiciousTimingThreshold = 100;   // Milliseconds
const int kMaxSuspiciousEvents = 5;

// Global Variables
HHOOK keyboard_hook;                            // Handle to the keyboard hook
std::ofstream log_file;                         // Log file stream
std::chrono::steady_clock::time_point last_key_press_time; // Timestamp of the last key press
std::vector<int> recent_intervals;              // Stores intervals between recent key presses

/**
 * @brief Retrieves the current date and time as a formatted string.
 *
 * @return std::string The current date and time in "YYYY-MM-DD HH:MM:SS" format,
 *                      or "Unknown Time" if unable to retrieve.
 */
std::string GetCurrentDateTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&now_time);
    
    if (!now_tm) {
        return "Unknown Time";
    }
    
    std::ostringstream oss;
    oss << std::put_time(now_tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

/**
 * @brief Converts a virtual key code to its corresponding key name.
 *
 * Handles alphabetical keys (A-Z), numerical keys (0-9), numpad keys,
 * and a set of predefined special keys. Returns "Unknown" for unrecognized key codes.
 *
 * @param vk_code The virtual key code to convert.
 * @return std::string The name of the key.
 */
std::string GetKeyName(DWORD vk_code) {
    // Handle alphabetical keys (A-Z)
    if (vk_code >= 0x41 && vk_code <= 0x5A) {  // A-Z
        char key_char = static_cast<char>(vk_code);
        return std::string(1, key_char);
    }

    // Handle numerical keys (0-9)
    if (vk_code >= 0x30 && vk_code <= 0x39) {  // 0-9
        char key_char = static_cast<char>(vk_code);
        return std::string(1, key_char);
    }

    // Handle numpad keys (Numpad0-Numpad9)
    if (vk_code >= VK_NUMPAD0 && vk_code <= VK_NUMPAD9) {
        return "Numpad" + std::to_string(vk_code - VK_NUMPAD0);
    }

    // Handle special keys
    switch (vk_code) {
        case VK_LWIN:
            return "Left Windows";
        case VK_RWIN:
            return "Right Windows";
        case VK_SHIFT:
            return "Shift";
        case VK_LSHIFT:
            return "Left Shift";
        case VK_RSHIFT:
            return "Right Shift";
        case VK_CONTROL:
            return "Control";
        case VK_LCONTROL:
            return "Left Control";
        case VK_RCONTROL:
            return "Right Control";
        case VK_MENU:
            return "Alt";
        case VK_LMENU:
            return "Left Alt";
        case VK_RMENU:
            return "Right Alt";
        case VK_TAB:
            return "Tab";
        case VK_CAPITAL:
            return "Caps Lock";
        case VK_ESCAPE:
            return "Escape";
        case VK_SPACE:
            return "Space";
        case VK_RETURN:
            return "Enter";
        case VK_BACK:
            return "Backspace";
        case VK_DELETE:
            return "Delete";
        case VK_INSERT:
            return "Insert";
        case VK_HOME:
            return "Home";
        case VK_END:
            return "End";
        case VK_PRIOR:
            return "Page Up";
        case VK_NEXT:
            return "Page Down";
        case VK_LEFT:
            return "Left Arrow";
        case VK_RIGHT:
            return "Right Arrow";
        case VK_UP:
            return "Up Arrow";
        case VK_DOWN:
            return "Down Arrow";
        default:
            return "Unknown";
    }
}

/**
 * @brief Checks for potential keystroke injection attacks based on timing intervals.
 *
 * Maintains a sliding window of recent intervals and counts how many fall below
 * the suspicious timing threshold. If the count exceeds the maximum allowed
 * suspicious events, an alert is logged, and the user is notified.
 *
 * @param interval The interval in milliseconds since the last key press.
 */
void CheckForAttack(int interval) {
    // Add the current interval to the list of recent intervals
    recent_intervals.push_back(interval);

    // Ensure the list does not exceed the maximum number of suspicious events
    if (recent_intervals.size() > kMaxSuspiciousEvents) {
        recent_intervals.erase(recent_intervals.begin());
    }

    // Count the number of intervals below the suspicious threshold
    int suspicious_count = 0;
    for (int i : recent_intervals) {
        if (i < kSuspiciousTimingThreshold) {
            suspicious_count++;
        }
    }

    // If the count meets or exceeds the maximum allowed, trigger an alert
    if (suspicious_count >= kMaxSuspiciousEvents) {
        std::string alert = "[" + GetCurrentDateTime() + "] ALERT: POTENTIAL KEYSTROKE INJECTION ATTACK DETECTED!";

        // Log the alert to the log file
        log_file << alert << std::endl;

        // Also output the alert to the standard error
        std::cerr << alert << std::endl;

        // Display a message box to notify the user
        MessageBoxA(NULL, "Potential keystroke injection attack detected!", "Security Alert", MB_ICONWARNING);
    }
}

/**
 * @brief Callback function for the keyboard hook.
 *
 * Processes key press events, logs them, and checks for suspicious timing patterns.
 *
 * @param n_code Specifies a code the hook procedure uses to determine how to process the message.
 * @param w_param Specifies the identifier of the keyboard message.
 * @param l_param Specifies a pointer to a KBDLLHOOKSTRUCT structure.
 * @return LRESULT Passes the hook information to the next hook procedure in the current hook chain.
 */
LRESULT CALLBACK KeyStrokeLogger(int n_code, WPARAM w_param, LPARAM l_param) {
    // Proceed only if n_code is HC_ACTION and the message is a key down event
    if (n_code == HC_ACTION && (w_param == WM_KEYDOWN || w_param == WM_SYSKEYDOWN)) {
        // Retrieve the keyboard event data
        KBDLLHOOKSTRUCT* kbd_struct = reinterpret_cast<KBDLLHOOKSTRUCT*>(l_param);
        DWORD vk_code = kbd_struct->vkCode;

        // Convert the virtual key code to a human-readable key name
        std::string key_name = GetKeyName(vk_code);

        // Capture the current time and calculate the interval since the last key press
        auto now = std::chrono::steady_clock::now();
        auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_key_press_time).count();
        last_key_press_time = now;

        // Check for potential injection attacks based on the interval
        CheckForAttack(interval);

        // Log the key press event if the log file is open
        if (log_file.is_open()) {
            log_file << "[" << GetCurrentDateTime() << "] "
                     << "Key Pressed: " << key_name << " (" << vk_code << ") - Interval: "
                     << interval << "ms " << std::endl;
        }

        // Optional: Update the UI with the last key press information
        // UpdateUI("Last key: " + key_name + " (" + std::to_string(vk_code) + ") - Interval: " + std::to_string(interval) + "ms");
    }

    // Pass the event to the next hook in the chain
    return CallNextHookEx(keyboard_hook, n_code, w_param, l_param);
}

/**
 * @brief Entry point of the program.
 *
 * Sets up the keyboard hook, opens the log file, and enters the message loop.
 * Cleans up resources upon termination.
 *
 * @return int Returns 0 on successful execution, or 1 if an error occurs.
 */
int main() {
    // Open the log file in append mode
    log_file.open("keys.log", std::ios::out | std::ios::app);
    if (!log_file) {
        std::cerr << "Failed to open log file!" << std::endl;
        return 1;
    }

    // Initialize the timestamp of the last key press to the current time
    last_key_press_time = std::chrono::steady_clock::now();

    // Install the low-level keyboard hook
    keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyStrokeLogger, GetModuleHandle(NULL), 0);
    if (!keyboard_hook) {
        std::cerr << "Failed to install hook!" << std::endl;
        log_file.close();
        return 1;
    }

    std::cout << "Keystroke logger started. Press Ctrl+C to stop." << std::endl;

    // Enter the message loop to keep the hook active
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Uninstall the keyboard hook and close the log file before exiting
    UnhookWindowsHookEx(keyboard_hook);
    log_file.close();

    return 0;
}