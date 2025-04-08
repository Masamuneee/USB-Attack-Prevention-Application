#ifndef KEY_LOGGER_H
#define KEY_LOGGER_H

#include <windows.h>
#include <hidusage.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <vector>
#include <string>
#include <unordered_map>
#include <sstream>

// Structure to store device-specific information
struct KeyboardDeviceInfo {
    std::string deviceName;
    std::string deviceId;
    std::chrono::steady_clock::time_point lastKeyPressTime;
    std::vector<int> recentIntervals;
    
    KeyboardDeviceInfo() {}
    KeyboardDeviceInfo(const std::string& name, const std::string& id) 
        : deviceName(name), deviceId(id) {}
};

class KeyLogger
{
private:
    // Handle to the keyboard hook
    HHOOK keyboardHook;
    
    // Raw input device handle to keyboard mapping
    std::unordered_map<HANDLE, KeyboardDeviceInfo> deviceMap;
    
    // Last active device handle
    HANDLE lastActiveDevice;

    // File stream for saving logged key information
    std::ofstream logFile;

    // Records the time of the last key press
    std::chrono::steady_clock::time_point lastKeyPressTime;

    // Stores intervals (in ms) between consecutive key presses
    std::vector<int> recentIntervals;

    // Static callback function for the low-level keyboard hook
    static LRESULT CALLBACK KeyStrokeLogger(int nCode, WPARAM wParam, LPARAM lParam);
    
    // Static window procedure for raw input
    static LRESULT CALLBACK RawInputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Helper method to log the key press to file
    void LogKeyStroke(DWORD vkCode, int interval, const std::string& deviceName);

    // Optional method to check for suspicious timing patterns
    void CheckForSuspiciousActivity(int interval, const std::string& deviceId);
    
    // Raw input registration and handling
    HWND rawInputWindow;
    void RegisterRawInput();
    void ProcessRawInput(HRAWINPUT hRawInput);
    std::string GetDeviceName(HANDLE deviceHandle);

public:
    KeyLogger();
    ~KeyLogger();

    // Singleton pattern
    static KeyLogger& GetInstance();

    // Install the keyboard hook and open the log file
    void Start();

    // Remove the keyboard hook and close the log file
    void Stop();
    
    // Get the device that generated a specific key event
    HANDLE GetDeviceHandleFromVirtualKey(DWORD vkCode);
};

#endif // KEY_LOGGER_H