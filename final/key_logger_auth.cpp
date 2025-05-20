#include <windows.h>
#include <string>
#include <iostream>

// Global variables for authentication mode
static bool captureEnabled = false;
static std::string capturedInput;
static std::string authDeviceId;
static HWND authDialog = NULL;
static HHOOK authKeyboardHook = NULL;
static const int MAX_AUTH_INPUT_LENGTH = 6; // Limit to 6 digits for authentication code

// Function prototypes
LRESULT CALLBACK AuthKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

// Functions called by DeviceAuthenticator - no extern "C" here
void StartKeyboardCapture(const std::string& deviceId, HWND dialogWindow)
{
    captureEnabled = true;
    capturedInput.clear();
    authDeviceId = deviceId;
    authDialog = dialogWindow;
    
    // Install a low-level keyboard hook to capture keystrokes
    authKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, AuthKeyboardProc, 
                                      GetModuleHandle(NULL), 0);
    
    if (!authKeyboardHook) {
        std::cerr << "Failed to install authentication keyboard hook" << std::endl;
    }
    
    std::cerr << "Started keyboard capture for authentication of device: " << deviceId << std::endl;
}

void StopKeyboardCapture()
{
    if (authKeyboardHook) {
        UnhookWindowsHookEx(authKeyboardHook);
        authKeyboardHook = NULL;
    }
    
    captureEnabled = false;
    capturedInput.clear();
    authDeviceId.clear();
    authDialog = NULL;
    
    std::cerr << "Stopped keyboard capture" << std::endl;
}

// Low-level keyboard hook procedure for authentication with enhanced security
LRESULT CALLBACK AuthKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && captureEnabled && authDialog) {
        KBDLLHOOKSTRUCT* kbStruct = (KBDLLHOOKSTRUCT*)lParam;
        
        // Only process key down events
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            DWORD vkCode = kbStruct->vkCode;
            
            // Handle digit keys (0-9)
            if ((vkCode >= '0' && vkCode <= '9') || 
                (vkCode >= VK_NUMPAD0 && vkCode <= VK_NUMPAD9)) {
                
                // Only allow up to MAX_AUTH_INPUT_LENGTH digits
                if (capturedInput.length() >= MAX_AUTH_INPUT_LENGTH) {
                    // Completely full - block any more input and alert user
                    std::cerr << "Auth input limit reached, blocking additional input" << std::endl;
                    
                    // Optionally, we could show a visual indicator or play a sound in the dialog
                    if (IsWindow(authDialog)) {
                        // Send a message to update the UI with a "MAX INPUT REACHED" notification
                        char* inputCopy = _strdup(capturedInput.c_str());
                        if (inputCopy) {
                            // Use the existing message channel but add a flag to indicate max input reached
                            PostMessage(authDialog, WM_USER + 201, 0, (LPARAM)inputCopy);
                        }
                    }
                    
                    // Block the keystroke
                    return 1;
                }
                
                // Convert to digit character
                char digit;
                if (vkCode >= '0' && vkCode <= '9') {
                    digit = static_cast<char>(vkCode);
                } else {
                    // Convert numpad keys to digits
                    digit = '0' + (vkCode - VK_NUMPAD0);
                }
                
                // Add to captured input
                capturedInput += digit;
                std::cerr << "Auth input now: " << capturedInput << std::endl;
                
                // Update the authentication dialog with the current input
                if (IsWindow(authDialog)) {
                    char* inputCopy = _strdup(capturedInput.c_str());
                    if (inputCopy) {
                        PostMessage(authDialog, WM_USER + 200, 0, (LPARAM)inputCopy);
                    }
                    
                    // If we've reached exactly 6 digits, automatically submit for verification
                    if (capturedInput.length() == MAX_AUTH_INPUT_LENGTH) {
                        // Notify dialog that we've reached max input - it can decide to auto-validate
                        PostMessage(authDialog, WM_USER + 202, 0, 0);
                    }
                }
                
                // Block the keystroke from reaching applications
                return 1;
            }
            // Handle backspace key
            else if (vkCode == VK_BACK && !capturedInput.empty()) {
                capturedInput.pop_back();
                std::cerr << "Auth input (after backspace): " << capturedInput << std::endl;
                
                // Update the authentication dialog
                if (IsWindow(authDialog)) {
                    char* inputCopy = _strdup(capturedInput.c_str());
                    if (inputCopy) {
                        PostMessage(authDialog, WM_USER + 200, 0, (LPARAM)inputCopy);
                    }
                }
                
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }
            // Handle escape key (cancel authentication)
            else if (vkCode == VK_ESCAPE) {
                // Cancel the authentication via the dialog's window proc
                if (IsWindow(authDialog)) {
                    PostMessage(authDialog, WM_COMMAND, IDCANCEL, 0);
                }
                
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }
            // Handle enter key (submit current input)
            else if (vkCode == VK_RETURN && !capturedInput.empty()) {
                // Let the dialog process the current input
                if (IsWindow(authDialog)) {
                    // Create a copy of the current input
                    char* inputCopy = _strdup(capturedInput.c_str());
                    if (inputCopy) {
                        // Send it to the dialog for processing
                        PostMessage(authDialog, WM_USER + 200, 1, (LPARAM)inputCopy); // 1 = final submit
                    }
                }
                
                // Block the keystroke
                return 1;
            }
            // Block all other keys during authentication
            else {
                // Block any non-numeric key except those we've already handled
                return 1;
            }
        }
    }
    
    // Call the next hook in the chain
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}
