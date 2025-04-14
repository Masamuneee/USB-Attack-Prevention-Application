#ifndef KEY_LOGGER_H
#define KEY_LOGGER_H

#include <windows.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <sstream>

class KeyLogger
{
private:
    // Handle to the keyboard hook
    HHOOK keyboardHook;

    // File stream for saving logged key information
    std::ofstream logFile;

    // Records the time of the last key press
    std::chrono::steady_clock::time_point lastKeyPressTime;

    // Stores intervals (in ms) between consecutive key presses
    std::vector<int> recentIntervals;

    // Static callback function for the low-level keyboard hook
    static LRESULT CALLBACK KeyStrokeLogger(int nCode, WPARAM wParam, LPARAM lParam);

    // Helper method to log the key press to file
    void LogKeyStroke(DWORD vkCode, int interval);

public:
    KeyLogger();
    ~KeyLogger();

    // Singleton pattern
    static KeyLogger& GetInstance();

    // Install the keyboard hook and open the log file
    void Start();

    // Remove the keyboard hook and close the log file
    void Stop();
};

#endif // KEY_LOGGER_H
