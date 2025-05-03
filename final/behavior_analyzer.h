#ifndef BEHAVIOR_ANALYZER_H
#define BEHAVIOR_ANALYZER_H

// Fix potential _WIN32_WINNT redefinition issues
// https://stackoverflow.com/questions/17447956/c-winapi-raw-input-specific-functions-and-structures-missing
#ifdef __MINGW32__
#   ifndef _WIN32_WINNT
#       define _WIN32_WINNT 0x0501
#   endif
#endif

#include "device_authenticator.h"
// Windows API includes
#include <windows.h>
#include <winuser.h>
// Standard library includes
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

// Raw input definitions if not already defined
#ifndef WM_INPUT
#define WM_INPUT 0x00FF
#endif

#ifndef RIM_TYPEKEYBOARD
#define RIM_TYPEKEYBOARD 1
#endif

#ifndef RIDEV_INPUTSINK
#define RIDEV_INPUTSINK 0x00000100
#endif

#ifndef RIDI_DEVICENAME
#define RIDI_DEVICENAME 0x20000007
#endif

// Event types that can be detected by the analyzer
enum class AnalyzerEvent {
	kSuspiciousTypingDetected,
	kBlacklistedWordDetected,
	kBlacklistedWordPartialMatch
};

// Structure to track per-device behavior data
struct DeviceBehaviorData {
	std::vector<int> key_press_intervals;
	std::string current_input;
	std::chrono::steady_clock::time_point last_key_press_time;
	std::string device_name;
	std::string device_id;
	DWORD last_key_code;          // Track the last key pressed
	int repeated_key_count;       // Count consecutive repeated keys

	DeviceBehaviorData() {
		last_key_press_time = std::chrono::steady_clock::now();
		last_key_code = 0;
		repeated_key_count = 0;
	}
};
// Interface for objects that want to receive analyzer events
class IAnalyzerListener {
	public:
		virtual ~IAnalyzerListener() = default;
		virtual void OnAnalyzerEvent(AnalyzerEvent event, const std::string& device_id = "") = 0;
};

class BehaviorAnalyzer {
	private:
		// Window handle for Raw Input processing
		HWND message_window_;

		// File stream for saving debug information
		std::ofstream log_file_;
			
		// Window class name for the hidden message window
		static const char* kAnalyzerWindowClass;
			
		// Window procedure for Raw Input messages
		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
			
		// Map to track behavior data for each device
		std::unordered_map<std::string, DeviceBehaviorData> device_data_map_;

		// List of user-defined forbidden words to detect
		std::set<std::string> blacklisted_words_;
			
		// List of event listeners
		std::vector<IAnalyzerListener*> listeners_;
			
		// Option to block suspicious input
		bool block_suspicious_input_;

		// Process a keystroke for a specific device
		void ProcessKeystroke(const std::string& device_id, DWORD vk_code);

		// Analyze the current keystroke (e.g., build or clear current_input)
		void AnalyzeKeystrokes(const std::string& device_id, DWORD vk_code);

		// Check for suspicious activity
		void CheckForSuspiciousActivity(const std::string& device_id, int interval);

		// Check if the current_input contains a blacklisted word
		bool ContainsBlacklistedWord(const std::string& input);
			
		// Notify registered listeners of events
		void NotifyListeners(AnalyzerEvent event, const std::string& device_id = "");
			
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
		static std::string GetDeviceNameFromHandle(HANDLE h_device);
};

#endif  // BEHAVIOR_ANALYZER_H