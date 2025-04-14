#ifndef BEHAVIOR_ANALYZER_H
#define BEHAVIOR_ANALYZER_H

#include <windows.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// Event types that can be detected by the analyzer
enum class AnalyzerEvent {
    SUSPICIOUS_TYPING_DETECTED,
    BLACKLISTED_WORD_DETECTED,
    BLACKLISTED_WORD_PARTIAL_MATCH
};

// Structure to track per-device behavior data
struct DeviceBehaviorData {
    std::vector<int> keyPressIntervals;
    std::string currentInput;
    std::chrono::steady_clock::time_point lastKeyPressTime;
    std::string deviceName;
    std::string deviceId;

    DeviceBehaviorData() {
        lastKeyPressTime = std::chrono::steady_clock::now();
    }
};

// Interface for objects that want to receive analyzer events
class IAnalyzerListener {
public:
    virtual ~IAnalyzerListener() = default;
    virtual void OnAnalyzerEvent(AnalyzerEvent event, const std::string& deviceId = "") = 0;
};

class BehaviorAnalyzer
{
private:
    // Window handle for Raw Input processing
    HWND messageWindow;

    // File stream for saving debug information
    std::ofstream logFile;
    
    // Window class name for the hidden message window
    static const char* ANALYZER_WINDOW_CLASS;
    
    // Window procedure for Raw Input messages
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    // Map to track behavior data for each device
    std::unordered_map<std::string, DeviceBehaviorData> deviceDataMap;

    // List of user-defined forbidden words to detect
    std::set<std::string> blacklistedWords;
    
    // List of event listeners
    std::vector<IAnalyzerListener*> listeners;
    
    // Option to block suspicious input
    bool blockSuspiciousInput;

    // Process a keystroke for a specific device
    void ProcessKeystroke(const std::string& deviceId, DWORD vkCode);

    // Analyze the current keystroke (e.g., build or clear currentInput)
    void AnalyzeKeystrokes(const std::string& deviceId, DWORD vkCode);

    // Check for suspicious activity
    void CheckForSuspiciousActivity(const std::string& deviceId, int interval);

    // Check if the currentInput contains a blacklisted word
    bool ContainsBlacklistedWord(const std::string& input);
    
    // Notify registered listeners of events
    void NotifyListeners(AnalyzerEvent event, const std::string& deviceId = "");
    
    // Register for Raw Input
    void RegisterRawInput();

public:
    BehaviorAnalyzer();
    ~BehaviorAnalyzer();
    
    // Singleton pattern
    static BehaviorAnalyzer& GetInstance();

    // Start monitoring for suspicious behavior
    void Start();

    // Stop monitoring
    void Stop();

    // Allows adding words to the forbidden list at runtime
    void AddBlacklistedWord(const std::string& word);
    
    // Register/unregister for analyzer events
    void RegisterListener(IAnalyzerListener* listener);
    void UnregisterListener(IAnalyzerListener* listener);
    
    // Configuration for blocking suspicious input
    void SetBlockSuspiciousInput(bool block);
    bool GetBlockSuspiciousInput() const;
    
    // Get device name from handle (helper method)
    static std::string GetDeviceNameFromHandle(HANDLE hDevice);
};

#endif // BEHAVIOR_ANALYZER_H