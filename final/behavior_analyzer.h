#ifndef BEHAVIOR_ANALYZER_H
#define BEHAVIOR_ANALYZER_H

#include <windows.h>
#include <vector>
#include <set>
#include <string>

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

    // Analyze the current keystroke (e.g., build or clear currentInput)
    void AnalyzeKeystrokes(DWORD vkCode);

    // Determine if an incoming interval is suspiciously fast
    bool IsSuspiciousPattern(int interval);

    // Check if the currentInput contains a blacklisted word
    bool ContainsBlacklistedWord();

public:
    BehaviorAnalyzer();
    ~BehaviorAnalyzer();

    // Installs the low-level hook to watch for suspicious behavior
    void Start();

    // Uninstalls the hook
    void Stop();

    // Allows adding words to the forbidden list at runtime
    void AddBlacklistedWord(const std::string& word);
};

#endif // BEHAVIOR_ANALYZER_H
