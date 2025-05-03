#include <windows.h>
#include <string>
#include <iostream>

// Global variables for authentication mode
static bool captureEnabled = false;
static std::string capturedInput;
static std::string authDeviceId;
static HWND authDialog = NULL;
static HHOOK authKeyboardHook = NULL;

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

// Low-level keyboard hook procedure for authentication
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
                
                char digit;
                if (vkCode >= '0' && vkCode <= '9') {
                    digit = static_cast<char>(vkCode);
                } else {
                    // Convert numpad keys to digits
                    digit = '0' + (vkCode - VK_NUMPAD0);
                }
                
                capturedInput += digit;
                std::cerr << "Auth input now: " << capturedInput << std::endl;
                
                // Update the authentication dialog with the current input
                if (IsWindow(authDialog)) {
                    char* inputCopy = _strdup(capturedInput.c_str());
                    if (inputCopy) {
                        PostMessage(authDialog, WM_USER + 200, 0, (LPARAM)inputCopy);
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
        }
    }
    
    // Call the next hook in the chain
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}
