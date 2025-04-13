#ifndef BEHAVIOR_ANALYZER_H
#define BEHAVIOR_ANALYZER_H

#include <windows.h>
#include <vector>
#include <set>
#include <string>
#include <algorithm>

// Event types that can be detected by the analyzer
enum class AnalyzerEvent {
    SUSPICIOUS_TYPING_DETECTED,
    BLACKLISTED_WORD_DETECTED,
    BLACKLISTED_WORD_PARTIAL_MATCH
};

// Interface for objects that want to receive analyzer events
class IAnalyzerListener {
public:
    virtual ~IAnalyzerListener() = default;
    virtual void OnAnalyzerEvent(AnalyzerEvent event) = 0;
};

class BehaviorAnalyzer
{
private:
    // Static callback function for the low-level keyboard hook
    static LRESULT CALLBACK BehaviorMonitor(int nCode, WPARAM wParam, LPARAM lParam);

    // Stores intervals between key presses
    std::vector<int> keyPressIntervals;

    // List of user-defined forbidden words to detect
    std::set<std::string> blacklistedWords;

    // Accumulates typed characters to check for blacklisted words
    std::string currentInput;
    
    // List of event listeners
    std::vector<IAnalyzerListener*> listeners;
    
    // Option to block suspicious input
    bool blockSuspiciousInput;

    // Analyze the current keystroke (e.g., build or clear currentInput)
    void AnalyzeKeystrokes(DWORD vkCode);

    // Determine if an incoming interval is suspiciously fast
    bool IsSuspiciousPattern(int interval);

    // Check if the currentInput contains a blacklisted word
    bool ContainsBlacklistedWord();
    
    // Notify registered listeners of events
    void NotifyListeners(AnalyzerEvent event);
    
    // Hook handle
    HHOOK keyboardHook;

public:
    BehaviorAnalyzer();
    ~BehaviorAnalyzer();
    
    // Singleton pattern
    static BehaviorAnalyzer& GetInstance();

    // Installs the low-level hook to watch for suspicious behavior
    void Start();

    // Uninstalls the hook
    void Stop();

    // Allows adding words to the forbidden list at runtime
    void AddBlacklistedWord(const std::string& word);
    
    // Register/unregister for analyzer events
    void RegisterListener(IAnalyzerListener* listener);
    void UnregisterListener(IAnalyzerListener* listener);
    
    // Configuration for blocking suspicious input
    void SetBlockSuspiciousInput(bool block);
    bool GetBlockSuspiciousInput() const;
};

#endif // BEHAVIOR_ANALYZER_H
