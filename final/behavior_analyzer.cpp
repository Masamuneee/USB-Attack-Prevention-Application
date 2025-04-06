#include "behavior_analyzer.h"
#include <chrono>
#include <iostream>
#include <sstream>

// We'll need a static time or store it globally for intervals
static std::chrono::steady_clock::time_point g_lastPress = std::chrono::steady_clock::now();

// Helper to convert vkCode to a char if possible (extremely simplified)
static char VkToChar(DWORD vkCode)
{
    // Basic alpha-numeric assumption
    if ((vkCode >= 0x30 && vkCode <= 0x39) || (vkCode >= 0x41 && vkCode <= 0x5A)) {
        return static_cast<char>(vkCode);
    }
    return '\0';
}

LRESULT CALLBACK BehaviorAnalyzer::BehaviorMonitor(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        auto* kbdStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        DWORD vkCode = kbdStruct->vkCode;

        auto now = std::chrono::steady_clock::now();
        int interval = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastPress).count();
        g_lastPress = now;

        // Use the singleton instance
        BehaviorAnalyzer& analyzer = BehaviorAnalyzer::GetInstance();
        analyzer.AnalyzeKeystrokes(vkCode);

        if (analyzer.IsSuspiciousPattern(interval)) {
            std::cerr << "[BehaviorAnalyzer] Suspicious typing speed detected.\n";
            
            // Broadcast notification to registered listeners
            analyzer.NotifyListeners(AnalyzerEvent::SUSPICIOUS_TYPING_DETECTED);
            
            // Block the keystroke by returning 1 if configured to do so
            if (analyzer.blockSuspiciousInput) {
                return 1; // Block this keystroke
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

// Singleton implementation
BehaviorAnalyzer& BehaviorAnalyzer::GetInstance()
{
    static BehaviorAnalyzer instance;
    return instance;
}

BehaviorAnalyzer::BehaviorAnalyzer()
    : keyboardHook(nullptr), blockSuspiciousInput(false)
{
}

BehaviorAnalyzer::~BehaviorAnalyzer()
{
    Stop();
}

void BehaviorAnalyzer::Start()
{
    // Install the low-level keyboard hook
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, BehaviorMonitor, GetModuleHandle(nullptr), 0);
}

void BehaviorAnalyzer::Stop()
{
    if (keyboardHook) {
        UnhookWindowsHookEx(keyboardHook);
        keyboardHook = nullptr;
    }
}

void BehaviorAnalyzer::AddBlacklistedWord(const std::string& word)
{
    blacklistedWords.insert(word);
}

void BehaviorAnalyzer::AnalyzeKeystrokes(DWORD vkCode)
{
    char c = VkToChar(vkCode);
    // If non-alphanumeric, treat as potential delimiter
    if (c == '\0') {
        if (!currentInput.empty()) {
            // Once we see a delimiter, check the entire chunk
            if (ContainsBlacklistedWord()) {
                std::cerr << "[BehaviorAnalyzer] Blacklisted word detected in: "
                          << currentInput << "\n";
                
                // Notify listeners of the blacklisted word detection
                NotifyListeners(AnalyzerEvent::BLACKLISTED_WORD_DETECTED);
            }
            currentInput.clear();
        }
        return;
    }

    // Append to the current input buffer
    currentInput.push_back(c);

    // Optionally do partial checks for blacklisted words
    if (ContainsBlacklistedWord()) {
        std::cerr << "[BehaviorAnalyzer] Blacklisted word partially matched in: "
                  << currentInput << "\n";
        
        // Notify listeners of the partial match
        NotifyListeners(AnalyzerEvent::BLACKLISTED_WORD_PARTIAL_MATCH);
    }
}

bool BehaviorAnalyzer::IsSuspiciousPattern(int interval)
{
    // Keep track of intervals
    keyPressIntervals.push_back(interval);
    if (keyPressIntervals.size() > 10) {
        keyPressIntervals.erase(keyPressIntervals.begin());
    }

    // Example: if 5 intervals < 20 ms, consider suspicious
    int fastCount = 0;
    for (int i : keyPressIntervals) {
        if (i < 20) {
            fastCount++;
        }
    }
    return (fastCount >= 5);
}

bool BehaviorAnalyzer::ContainsBlacklistedWord()
{
    for (const auto& word : blacklistedWords) {
        if (currentInput.find(word) != std::string::npos) {
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

void BehaviorAnalyzer::NotifyListeners(AnalyzerEvent event)
{
    for (auto listener : listeners) {
        listener->OnAnalyzerEvent(event);
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
