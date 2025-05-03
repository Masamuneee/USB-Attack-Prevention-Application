// Include the header first, which has the GUID declaration
#include "device_authenticator.h"

// Microsoft Visual C++ uses pragma comment
#ifdef _MSC_VER
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "comctl32.lib")
#endif

// For MinGW, you need to add -lsetupapi when compiling

// Define a custom dialog resource ID since we don't have a resource file
#define IDD_DIALOG_AUTH 1001

// Window class name for device notification handling
const char* DEVICE_WINDOW_CLASS = "USBDeviceNotificationClass";
// Message when a device has been authenticated
const UINT WM_DEVICE_AUTHENTICATED = WM_USER + 100;

// Function declarations for keyboard capture
void StartKeyboardCapture(const std::string& deviceId, HWND dialogWindow);
void StopKeyboardCapture();

// Utility function to generate a random 4-character code
static std::string GenerateChallengeCode()
{
    const char* charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    constexpr size_t length = 4;
    std::string result;
    result.reserve(length);

    srand((unsigned int)time(nullptr));
    for (size_t i = 0; i < length; ++i) {
        int randomIndex = rand() % 36;
        result.push_back(charset[randomIndex]);
    }
    return result;
}

// Extract device info from device interface details
static std::string GetDeviceIdFromDeviceInterface(HDEVINFO deviceInfoSet, PSP_DEVICE_INTERFACE_DATA deviceInterfaceData)
{
    PSP_DEVICE_INTERFACE_DETAIL_DATA deviceDetailData;
    DWORD requiredSize = 0;
    
    // Get the required size
    SetupDiGetDeviceInterfaceDetail(deviceInfoSet, deviceInterfaceData, nullptr, 0, &requiredSize, nullptr);
    
    // Allocate memory for the detail data
    deviceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);
    deviceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
    
    // Get the details
    if (!SetupDiGetDeviceInterfaceDetail(deviceInfoSet, deviceInterfaceData, deviceDetailData, requiredSize, nullptr, nullptr)) {
        free(deviceDetailData);
        return "";
    }
    
    std::string devicePath = deviceDetailData->DevicePath;
    free(deviceDetailData);
    
    return devicePath;
}

// Get friendly name for a device - Enhanced to include more details
static std::string GetDeviceFriendlyName(HDEVINFO deviceInfoSet, PSP_DEVINFO_DATA deviceInfoData)
{
    char buffer[256] = {0};
    DWORD bufferSize = sizeof(buffer);
    std::string deviceName = "Unknown Device";
    std::string deviceDesc = "";
    std::string manufacturer = "";
    
    // Try to get friendly name
    if (SetupDiGetDeviceRegistryProperty(deviceInfoSet, deviceInfoData, 
                                         SPDRP_FRIENDLYNAME, nullptr, 
                                         (BYTE*)buffer, bufferSize, nullptr)) {
        deviceName = buffer;
    }
    
    // Try to get device description
    memset(buffer, 0, bufferSize);
    if (SetupDiGetDeviceRegistryProperty(deviceInfoSet, deviceInfoData, 
                                     SPDRP_DEVICEDESC, nullptr, 
                                     (BYTE*)buffer, bufferSize, nullptr)) {
        deviceDesc = buffer;
        if (deviceName == "Unknown Device") {
            deviceName = deviceDesc; // Use description as fallback
        }
    }
    
    // Try to get device manufacturer
    memset(buffer, 0, bufferSize);
    if (SetupDiGetDeviceRegistryProperty(deviceInfoSet, deviceInfoData, 
                                     SPDRP_MFG, nullptr, 
                                     (BYTE*)buffer, bufferSize, nullptr)) {
        manufacturer = buffer;
    }
    
    // Get hardware IDs for VID/PID information
    std::string vidPid;
    memset(buffer, 0, bufferSize);
    if (SetupDiGetDeviceRegistryProperty(deviceInfoSet, deviceInfoData, 
                                     SPDRP_HARDWAREID, nullptr, 
                                     (BYTE*)buffer, bufferSize, nullptr)) {
        std::string hwid = buffer;
        size_t vidPos = hwid.find("VID_");
        size_t pidPos = hwid.find("PID_");
        
        if (vidPos != std::string::npos && pidPos != std::string::npos) {
            std::string vid = hwid.substr(vidPos, 8); // VID_xxxx
            std::string pid = hwid.substr(pidPos, 8); // PID_xxxx
            vidPid = vid + " " + pid;
        }
    }
    
    // Build a comprehensive device name with available information
    std::string fullName = deviceName;
    
    if (!manufacturer.empty() && fullName.find(manufacturer) == std::string::npos) {
        fullName += " [" + manufacturer + "]";
    }
    
    if (!vidPid.empty()) {
        fullName += " (" + vidPid + ")";
    }
    
    return fullName;
}

// C++ dialog for device authentication
LRESULT CALLBACK AuthDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static std::string challengeCode;
    static std::string enteredCode;
    static HWND hwndEdit;
    
    switch (message) {
        case WM_INITDIALOG: {
            // Store the challenge code passed in lParam
            challengeCode = *(std::string*)lParam;
            
            // Create controls
            CreateWindow("STATIC", ("Please enter code: " + challengeCode).c_str(),
                         WS_VISIBLE | WS_CHILD, 10, 10, 200, 20, hwnd, nullptr, nullptr, nullptr);
            
            hwndEdit = CreateWindow("EDIT", "",
                         WS_VISIBLE | WS_CHILD | WS_BORDER, 10, 40, 200, 20, 
                         hwnd, (HMENU)101, nullptr, nullptr);
                         
            CreateWindow("BUTTON", "Verify",
                         WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 70, 75, 30, 
                         hwnd, (HMENU)IDOK, nullptr, nullptr);
                         
            CreateWindow("BUTTON", "Cancel",
                         WS_VISIBLE | WS_CHILD, 95, 70, 75, 30, 
                         hwnd, (HMENU)IDCANCEL, nullptr, nullptr);
            
            return TRUE;
        }
        
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                char buffer[256];
                GetWindowText(hwndEdit, buffer, 256);
                enteredCode = buffer;
                
                // Convert both to uppercase for case-insensitive comparison
                std::transform(enteredCode.begin(), enteredCode.end(), enteredCode.begin(), ::toupper);
                
                EndDialog(hwnd, enteredCode == challengeCode ? IDOK : IDCANCEL);
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwnd, IDCANCEL);
                return TRUE;
            }
            break;
    }
    
    return FALSE;
}

// Enhanced prompt for authentication with proper dialog
static bool PromptUserForAuthentication(HWND parent, const std::string& deviceName, const std::string& code)
{
    // Create the dialog resources dynamically
    HWND hDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "STATIC",
        "USB Device Authentication",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 350, 200,
        parent, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    if (!hDlg) {
        return false;
    }
    
    // Center the dialog on the parent window or screen
    RECT rc, rcDlg, rcOwner;
    if (parent) {
        GetWindowRect(parent, &rcOwner);
    } else {
        rcOwner.left = rcOwner.top = 0;
        rcOwner.right = GetSystemMetrics(SM_CXSCREEN);
        rcOwner.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    
    GetWindowRect(hDlg, &rcDlg);
    rc.left = rcOwner.left + ((rcOwner.right - rcOwner.left) - (rcDlg.right - rcDlg.left)) / 2;
    rc.top = rcOwner.top + ((rcOwner.bottom - rcOwner.top) - (rcDlg.bottom - rcDlg.top)) / 2;
    SetWindowPos(hDlg, nullptr, rc.left, rc.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    
    // Create the device information text
    std::string infoText = "A new keyboard device has been detected:\n" + deviceName + 
                          "\n\nPlease type the following code on that keyboard to verify:";
    CreateWindowEx(0, "STATIC", infoText.c_str(),
                  WS_CHILD | WS_VISIBLE | SS_LEFT,
                  20, 20, 310, 60, hDlg, nullptr, GetModuleHandle(nullptr), nullptr);
    
    // Create the challenge code display (larger font for better visibility)
    HWND hCodeText = CreateWindowEx(0, "STATIC", code.c_str(),
                                   WS_CHILD | WS_VISIBLE | SS_CENTER,
                                   20, 90, 310, 30, hDlg, nullptr, GetModuleHandle(nullptr), nullptr);
                                   
    // Set larger font for the code
    HFONT hFont = CreateFont(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, 
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                            DEFAULT_QUALITY, DEFAULT_PITCH, "Arial");
    SendMessage(hCodeText, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // Create the verify and cancel buttons
    HWND hBtnVerify = CreateWindowEx(0, "BUTTON", "Verify",
                                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                    180, 140, 70, 25, hDlg, (HMENU)IDOK, GetModuleHandle(nullptr), nullptr);
                                    
    HWND hBtnCancel = CreateWindowEx(0, "BUTTON", "Cancel",
                                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                    260, 140, 70, 25, hDlg, (HMENU)IDCANCEL, GetModuleHandle(nullptr), nullptr);
    
    // Set the focus to the verify button
    SetFocus(hBtnVerify);
    
    // Message loop for the dialog
    MSG msg;
    BOOL result = FALSE;
    bool dialogResult = false;
    
    while ((result = GetMessage(&msg, nullptr, 0, 0)) != 0) {
        if (result == -1) {
            break;
        }
        
        // Handle button clicks
        if (msg.message == WM_COMMAND) {
            if (LOWORD(msg.wParam) == IDOK) {
                dialogResult = true;
                DestroyWindow(hDlg);
                break;
            } else if (LOWORD(msg.wParam) == IDCANCEL) {
                dialogResult = false;
                DestroyWindow(hDlg);
                break;
            }
        } else if (msg.message == WM_DESTROY && msg.hwnd == hDlg) {
            break;
        }
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    DeleteObject(hFont);
    return dialogResult;
}

// Static function for the authentication dialog window procedure
static LRESULT CALLBACK AuthenticationDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Move variable declarations outside the switch statement to fix the crossing initialization error
    DeviceAuthenticator* pAuth = NULL;
    char* input = NULL;
    std::string statusText;
    HWND hStatus = NULL;
    const char* authCode = NULL;
    const char* deviceId = NULL;

    switch (msg) {
        case WM_COMMAND:
            if (LOWORD(wParam) == IDCANCEL) {
                // Get the authenticator instance
                pAuth = (DeviceAuthenticator*)GetProp(hwnd, "DeviceAuthenticator");
                if (pAuth) {
                    pAuth->CancelAuthentication();
                }
                
                // Free stored properties
                free(GetProp(hwnd, "AuthCode"));
                free(GetProp(hwnd, "DeviceId"));
                RemoveProp(hwnd, "StatusText");
                RemoveProp(hwnd, "DeviceAuthenticator");
                RemoveProp(hwnd, "AuthCode");
                RemoveProp(hwnd, "DeviceId");
                
                DestroyWindow(hwnd);
                return TRUE;
            }
            break;
        
        case WM_DESTROY:
            // Make sure we clean up if the window is closed
            pAuth = (DeviceAuthenticator*)GetProp(hwnd, "DeviceAuthenticator");
            if (pAuth && pAuth->IsAuthenticating()) {
                pAuth->CancelAuthentication();
            }
            
            // Free stored properties
            free(GetProp(hwnd, "AuthCode"));
            free(GetProp(hwnd, "DeviceId"));
            RemoveProp(hwnd, "StatusText");
            RemoveProp(hwnd, "DeviceAuthenticator");
            RemoveProp(hwnd, "AuthCode");
            RemoveProp(hwnd, "DeviceId");
            break;
            
        case WM_USER + 200: // Custom message for input update
            // Update the status text with the current input
            input = (char*)lParam;
            if (input) {
                statusText = "Input: ";
                statusText += input;
                
                hStatus = (HWND)GetProp(hwnd, "StatusText");
                if (hStatus) {
                    SetWindowTextA(hStatus, statusText.c_str());
                }
                
                // Check if authentication is complete
                authCode = (const char*)GetProp(hwnd, "AuthCode");
                if (authCode && strcmp(input, authCode) == 0) {
                    // Authentication succeeded
                    pAuth = (DeviceAuthenticator*)GetProp(hwnd, "DeviceAuthenticator");
                    deviceId = (const char*)GetProp(hwnd, "DeviceId");
                    
                    if (pAuth && deviceId) {
                        // Call ProcessAuthInput with success
                        pAuth->ProcessAuthInput(input, deviceId);
                    }
                    
                    // Close the dialog
                    DestroyWindow(hwnd);
                }
                
                free(input); // Free the allocated input string
            }
            return TRUE;
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Static callback for the window procedure
LRESULT CALLBACK DeviceAuthenticator::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_DEVICECHANGE) {
        DeviceAuthenticator& authenticator = DeviceAuthenticator::GetInstance();
        
        switch (wParam) {
            case DBT_DEVICEARRIVAL: {
                // A device has been connected
                DEV_BROADCAST_HDR* hdr = (DEV_BROADCAST_HDR*)lParam;
                if (hdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                    DEV_BROADCAST_DEVICEINTERFACE* devInterface = (DEV_BROADCAST_DEVICEINTERFACE*)lParam;
                    std::string deviceId = devInterface->dbcc_name;
                    
                    // Check if it's a keyboard device (simplified)
                    if (deviceId.find("HID") != std::string::npos && 
                        (deviceId.find("Keyboard") != std::string::npos || 
                         deviceId.find("Vid_") != std::string::npos)) {
                        
                        if (!authenticator.IsDeviceBlocked(deviceId)) {
                            bool success = authenticator.AuthenticateDevice(deviceId);
                            if (!success) {
                                authenticator.BlockDevice(deviceId);
                            }
                        } else {
                            // Device is blocked, notify user
                            MessageBoxA(hwnd, "This device is currently blocked due to too many authentication failures.",
                                       "Blocked Device", MB_ICONWARNING);
                        }
                    }
                }
                break;
            }
            
            case DBT_DEVICEREMOVECOMPLETE: {
                // A device has been disconnected
                // Update internal state if needed
                break;
            }
        }
    } else if (uMsg == WM_DEVICE_AUTHENTICATED) {
        // Handle the authentication completed message
        // Could be used to update UI or notify other components
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Singleton implementation
DeviceAuthenticator& DeviceAuthenticator::GetInstance()
{
    static DeviceAuthenticator instance;
    return instance;
}

DeviceAuthenticator::DeviceAuthenticator()
    : messageWindow(nullptr), deviceNotifyHandle(nullptr), authenticationInProgress(false)
{
    // Determine path for trusted devices file
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    
    std::string exeDir = exePath;
    size_t lastSlash = exeDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        exeDir = exeDir.substr(0, lastSlash);
    }
    
    trustedDevicesPath = exeDir + "\\trusted_usb_devices.dat";
    std::cerr << "Trusted devices will be stored at: " << trustedDevicesPath << std::endl;
}

DeviceAuthenticator::~DeviceAuthenticator()
{
    Stop();
}

void DeviceAuthenticator::Start()
{
    // Register window class for device notifications
    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = WindowProc;
    wcex.hInstance = GetModuleHandle(nullptr);
    wcex.lpszClassName = DEVICE_WINDOW_CLASS;
    
    RegisterClassEx(&wcex);
    
    // Create a hidden window to receive device notifications
    messageWindow = CreateWindow(DEVICE_WINDOW_CLASS, "USB Device Monitor", 
                                WS_OVERLAPPED, 0, 0, 0, 0, 
                                HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr);
    
    if (messageWindow) {
        RegisterDeviceNotifications();
    }
    
    // Scan and authenticate existing devices
    EnumerateExistingDevices();
}

void DeviceAuthenticator::Stop()
{
    if (deviceNotifyHandle) {
        UnregisterDeviceNotification(deviceNotifyHandle);
        deviceNotifyHandle = nullptr;
    }
    
    if (messageWindow) {
        DestroyWindow(messageWindow);
        messageWindow = nullptr;
    }
    
    UnregisterClass(DEVICE_WINDOW_CLASS, GetModuleHandle(nullptr));
}

void DeviceAuthenticator::RegisterDeviceNotifications()
{
    // Set up the device interface class guid for HIDs (which includes keyboards)
    DEV_BROADCAST_DEVICEINTERFACE notificationFilter = {0};
    notificationFilter.dbcc_size = sizeof(notificationFilter);
    notificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    notificationFilter.dbcc_classguid = GUID_DEVINTERFACE_KEYBOARD;
    
    deviceNotifyHandle = RegisterDeviceNotification(
        messageWindow,
        &notificationFilter,
        DEVICE_NOTIFY_WINDOW_HANDLE
    );
}

void DeviceAuthenticator::EnumerateExistingDevices()
{
    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(
        &GUID_DEVINTERFACE_KEYBOARD,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );
    
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return;
    }
    
    SP_DEVICE_INTERFACE_DATA deviceInterfaceData = {0};
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    
    SP_DEVINFO_DATA deviceInfoData = {0};
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    
    // Enumerate all keyboard interfaces
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(deviceInfoSet, nullptr, &GUID_DEVINTERFACE_KEYBOARD, i, &deviceInterfaceData); i++) {
        std::string deviceId = GetDeviceIdFromDeviceInterface(deviceInfoSet, &deviceInterfaceData);
        
        if (!deviceId.empty() && SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData)) {
            std::string friendlyName = GetDeviceFriendlyName(deviceInfoSet, &deviceInfoData);
            std::string instanceId = GetDeviceInstanceId(deviceInfoSet, &deviceInfoData);
            
            // Create enhanced device info
            USBDeviceInfo deviceInfo(deviceId, friendlyName);
            deviceInfo.instanceId = instanceId;
            deviceInfo.hardwareId = GetDeviceHardwareId(deviceInfoSet, &deviceInfoData);
            deviceInfo.serialNumber = GetDeviceSerialNumber(deviceInfoSet, &deviceInfoData);
            
            // Check if this is a previously trusted device
            bool isTrusted = false;
            for (const auto& pair : knownDevices) {
                if (pair.second.authenticated && deviceInfo.IsSamePhysicalDevice(pair.second)) {
                    isTrusted = true;
                    break;
                }
            }
            
            if (isTrusted) {
                deviceInfo.authenticated = true;
                knownDevices[deviceId] = deviceInfo;
                std::cerr << "Found previously trusted device: " << friendlyName << std::endl;
                continue;
            }
            
            knownDevices[deviceId] = deviceInfo;
            // Create a detailed message showing device identification
            std::string message = "Do you trust this keyboard device?\n\n";
            message += "Device Name: " + friendlyName + "\n";
            message += "Device ID: " + deviceId + "\n";
            
            char locationInfo[256] = {0};
            if (SetupDiGetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData, SPDRP_LOCATION_INFORMATION, 
                                               NULL, (BYTE*)locationInfo, sizeof(locationInfo), NULL)) {
                message += "Location: " + std::string(locationInfo) + "\n";
            }
            
            message += "\nTrusting this device will allow it to send keystrokes to your system.\n"
                      "Untrusting will eject the device until it's manually restored.";
                      
            int result = MessageBoxA(nullptr, message.c_str(), "Device Trust", MB_YESNO | MB_ICONQUESTION);
            
            knownDevices[deviceId].authenticated = (result == IDYES);
            
            // If the device is not trusted, eject it automatically
            if (result != IDYES) {
                EjectDevice(deviceId);
            }
        }
    }
    
    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    
    // Save trusted devices after initial enumeration
    SaveTrustedDevices();
}

bool DeviceAuthenticator::AuthenticateDevice(const std::string& deviceId)
{
    if (authenticationInProgress) {
        std::cerr << "Cannot authenticate device " << deviceId 
                 << " - authentication already in progress" << std::endl;
        return false;
    }

    if (authenticationAttempts.find(deviceId) == authenticationAttempts.end()) {
        authenticationAttempts[deviceId] = 0;
    }

    if (authenticationAttempts[deviceId] >= 5) {
        // too many failures
        return false;
    }

    // Find or create the device info
    std::string deviceName = "Unknown Device";
    if (knownDevices.find(deviceId) == knownDevices.end()) {
        // Try to get a better name for the device
        HDEVINFO deviceInfoSet = SetupDiGetClassDevs(
            &GUID_DEVINTERFACE_KEYBOARD,
            nullptr,
            nullptr,
            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
        );
        
        if (deviceInfoSet != INVALID_HANDLE_VALUE) {
            SP_DEVINFO_DATA deviceInfoData = {0};
            deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
            
            for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); i++) {
                char buffer[256] = {0};
                if (SetupDiGetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData, 
                                                  SPDRP_FRIENDLYNAME, nullptr, 
                                                  (BYTE*)buffer, sizeof(buffer), nullptr) ||
                    SetupDiGetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData, 
                                                  SPDRP_DEVICEDESC, nullptr, 
                                                  (BYTE*)buffer, sizeof(buffer), nullptr)) {
                    deviceName = buffer;
                    break;
                }
            }
            
            SetupDiDestroyDeviceInfoList(deviceInfoSet);
        }
        
        knownDevices[deviceId] = USBDeviceInfo(deviceId, deviceName);
    } else {
        deviceName = knownDevices[deviceId].friendlyName;
    }
    
    // Check if this is a reconnection of a previously trusted device
    if (IsKnownTrustedDevice(deviceId)) {
        std::cerr << "Device " << deviceName << " recognized as previously trusted" << std::endl;
        
        knownDevices[deviceId].authenticated = true;
        knownDevices[deviceId].lastAuthAttempt = std::chrono::system_clock::now();
        
        // Save the updated trusted devices list
        SaveTrustedDevices();
        
        NotifyListeners(AuthEvent::DEVICE_AUTHENTICATED, deviceId);
        return true;
    }
    
    // Update last authentication attempt time
    knownDevices[deviceId].lastAuthAttempt = std::chrono::system_clock::now();

    // Generate 6-digit numeric code
    std::string authCode = GenerateAuthCode();
    std::cerr << "Generated auth code for " << deviceName << ": " << authCode << std::endl;
    
    // Set current authentication state
    authenticationInProgress = true;
    currentAuthDeviceId = deviceId;
    currentAuthCode = authCode;
    
    // Show the authentication dialog with the 6-digit code
    HWND hDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "STATIC",
        "USB Device Authentication",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 250,
        messageWindow, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    if (!hDlg) {
        // If dialog creation failed, try simpler method
        authenticationInProgress = false;
        bool userOk = PromptUserForAuthentication(messageWindow, deviceName, authCode);
        
        if (userOk) {
            knownDevices[deviceId].authenticated = true;
            SaveTrustedDevices();
            NotifyListeners(AuthEvent::DEVICE_AUTHENTICATED, deviceId);
            return true;
        } else {
            authenticationAttempts[deviceId]++;
            NotifyListeners(AuthEvent::DEVICE_AUTH_FAILED, deviceId);
            EjectDevice(deviceId);
            return false;
        }
    }
    
    // Center the dialog on screen
    RECT rc, rcDlg, rcDesktop;
    GetWindowRect(hDlg, &rcDlg);
    GetWindowRect(GetDesktopWindow(), &rcDesktop);
    
    int dlgWidth = rcDlg.right - rcDlg.left;
    int dlgHeight = rcDlg.bottom - rcDlg.top;
    
    int newX = (rcDesktop.right - dlgWidth) / 2;
    int newY = (rcDesktop.bottom - dlgHeight) / 2;
    
    SetWindowPos(hDlg, nullptr, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    
    // Create the message text
    std::string message = "A new USB keyboard device needs authentication:\n\n";
    message += "Device Name: " + deviceName + "\n";
    
    if (!knownDevices[deviceId].hardwareId.empty()) {
        message += "Hardware ID: " + knownDevices[deviceId].hardwareId + "\n";
    }
    
    message += "\nPlease type the following 6-digit code using the device keyboard to verify it:";
    
    CreateWindowEx(
        0, "STATIC", message.c_str(),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 20, 360, 100, hDlg, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    // Create the authentication code display with larger font
    HWND hCodeText = CreateWindowEx(0, "STATIC", authCode.c_str(),
                                   WS_CHILD | WS_VISIBLE | SS_CENTER,
                                   20, 120, 360, 40, hDlg, nullptr, GetModuleHandle(nullptr), nullptr);
    
    // Create a larger font for the code
    HFONT hFont = CreateFont(36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
    SendMessage(hCodeText, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // Create status text that will show the input as it's typed
    HWND hStatusText = CreateWindowEx(0, "STATIC", "Input: ",
                                     WS_CHILD | WS_VISIBLE | SS_LEFT,
                                     20, 170, 360, 20, hDlg, nullptr, GetModuleHandle(nullptr), nullptr);
    
    // Create cancel button
    HWND hCancel = CreateWindowEx(0, "BUTTON", "Cancel",
                                 WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 150, 200, 100, 30, hDlg, (HMENU)IDCANCEL, GetModuleHandle(nullptr), nullptr);
    
    // Store the dialog handle and this pointer
    SetProp(hDlg, "StatusText", (HANDLE)hStatusText);
    SetProp(hDlg, "DeviceAuthenticator", (HANDLE)this);
    SetProp(hDlg, "AuthCode", _strdup(authCode.c_str()));
    SetProp(hDlg, "DeviceId", _strdup(deviceId.c_str()));
    
    // Set the window procedure using our static function
    SetWindowLongPtr(hDlg, GWLP_WNDPROC, (LONG_PTR)AuthenticationDialogProc);
    
    // Start KeyLogger authentication capture mode - no extern "C" needed
    StartKeyboardCapture(deviceId, hDlg);
    
    SetFocus(hDlg);
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
    
    // The authentication result will be handled in the ProcessAuthInput method
    return false;
}

void DeviceAuthenticator::CancelAuthentication()
{
    if (!authenticationInProgress) {
        return;
    }
    
    std::string deviceId = currentAuthDeviceId;
    
    // Reset authentication state
    authenticationInProgress = false;
    currentAuthDeviceId.clear();
    currentAuthCode.clear();
    
    if (!deviceId.empty()) {
        // Increment authentication failures
        authenticationAttempts[deviceId]++;
        
        // Notify listeners
        NotifyListeners(AuthEvent::DEVICE_AUTH_FAILED, deviceId);
        
        // Eject the device
        EjectDevice(deviceId);
    }
    
    // Stop keyboard capture - no extern "C" needed
    StopKeyboardCapture();
}

bool DeviceAuthenticator::ProcessAuthInput(const std::string& input, const std::string& deviceId)
{
    if (!authenticationInProgress || deviceId != currentAuthDeviceId) {
        return false;
    }
    
    bool success = (input == currentAuthCode);
    
    if (success) {
        // Authentication successful
        knownDevices[deviceId].authenticated = true;
        
        // Save trusted devices list
        SaveTrustedDevices();
        
        // Notify listeners
        NotifyListeners(AuthEvent::DEVICE_AUTHENTICATED, deviceId);
    } else {
        // Authentication failed
        authenticationAttempts[deviceId]++;
        
        // Notify listeners
        NotifyListeners(AuthEvent::DEVICE_AUTH_FAILED, deviceId);
        
        // Eject the device
        EjectDevice(deviceId);
    }
    
    // Reset authentication state
    authenticationInProgress = false;
    currentAuthDeviceId.clear();
    currentAuthCode.clear();
    
    // Stop keyboard capture - no extern "C" needed
    StopKeyboardCapture();
    
    return success;
}

// Generate a 6-digit numeric code
std::string DeviceAuthenticator::GenerateAuthCode()
{
    std::string result;
    result.reserve(6);
    
    // Use current time and high-resolution clock for better randomness
    srand((unsigned int)time(nullptr) ^ 
          (unsigned int)std::chrono::high_resolution_clock::now().time_since_epoch().count());
    
    // Generate 6 random digits
    for (int i = 0; i < 6; i++) {
        result.push_back('0' + (rand() % 10)); // Digits 0-9
    }
    
    return result;
}

void DeviceAuthenticator::BlockDevice(const std::string& deviceId)
{
    // block for 1 hour
    blockedDevices[deviceId] = std::chrono::steady_clock::now() + std::chrono::hours(1);

    std::cerr << "[Authenticator] Device " << deviceId
              << " blocked for 1 hour due to authentication failures.\n";
              
    // Update device info
    if (knownDevices.find(deviceId) != knownDevices.end()) {
        knownDevices[deviceId].authenticated = false;
    }
}

bool DeviceAuthenticator::IsDeviceBlocked(const std::string& deviceId)
{
    auto it = blockedDevices.find(deviceId);
    if (it != blockedDevices.end()) {
        auto now = std::chrono::steady_clock::now();
        if (now < it->second) {
            // still blocked
            return true;
        } else {
            // un-block device
            blockedDevices.erase(it);
            authenticationAttempts.erase(deviceId);
            return false;
        }
    }
    return false;
}

std::vector<USBDeviceInfo> DeviceAuthenticator::GetConnectedDevices() const
{
    std::vector<USBDeviceInfo> devices;
    for (const auto& pair : knownDevices) {
        devices.push_back(pair.second);
    }
    return devices;
}

bool DeviceAuthenticator::SetDeviceTrust(const std::string& deviceId, bool trusted)
{
    auto it = knownDevices.find(deviceId);
    if (it != knownDevices.end()) {
        it->second.authenticated = trusted;
        
        // If we're trusting a previously blocked device, unblock it
        if (trusted) {
            blockedDevices.erase(deviceId);
            authenticationAttempts.erase(deviceId);
        }
        
        return true;
    }
    return false;
}

// New function implementation to untrust a device
bool DeviceAuthenticator::UntrustDevice(const std::string& deviceId)
{
    auto it = knownDevices.find(deviceId);
    if (it != knownDevices.end()) {
        it->second.authenticated = false;
        
        // Reset authentication attempts
        authenticationAttempts[deviceId] = 0;
        
        // Remove from blocked devices if it was blocked
        blockedDevices.erase(deviceId);
        
        // Notify listeners
        NotifyListeners(AuthEvent::DEVICE_UNTRUSTED, deviceId);
        
        // Automatically eject the device when it's untrusted
        EjectDevice(deviceId);
        
        return true;
    }
    return false;
}

void DeviceAuthenticator::RegisterListener(IDeviceAuthListener* listener)
{
    if (listener && std::find(listeners.begin(), listeners.end(), listener) == listeners.end()) {
        listeners.push_back(listener);
    }
}

void DeviceAuthenticator::UnregisterListener(IDeviceAuthListener* listener)
{
    listeners.erase(
        std::remove(listeners.begin(), listeners.end(), listener),
        listeners.end()
    );
}

void DeviceAuthenticator::NotifyListeners(AuthEvent event, const std::string& deviceId)
{
    for (auto listener : listeners) {
        listener->OnAuthEvent(event, deviceId);
    }
}

// Implementation of device ejection function with improved reliability
bool DeviceAuthenticator::EjectDevice(const std::string& deviceId)
{
    // Log the ejection attempt
    std::cerr << "Attempting to eject device: " << deviceId << std::endl;
    
    std::string instanceId = GetDeviceInstanceIdFromPath(deviceId);
    if (instanceId.empty()) {
        std::cerr << "Failed to get instance ID for device: " << deviceId << std::endl;
        return false;
    }
    
    std::cerr << "Using instance ID: " << instanceId << std::endl;
    
    bool ejectionSucceeded = false;
    
    // Try multiple approaches to disable the device
    
    // Approach 1: Use DEVCON-style programmatic device disabling
    // This approach uses PnP Configuration Manager API directly
    {
        DEVINST devInst = 0;
        CONFIGRET status;
        
        // Create a non-const copy of the string (required by the API)
        char* deviceIdCopy = _strdup(instanceId.c_str());
        if (!deviceIdCopy) {
            std::cerr << "Failed to allocate memory for device ID" << std::endl;
        } else {
            // Try to locate the device node
            status = CM_Locate_DevNodeA(&devInst, deviceIdCopy, CM_LOCATE_DEVNODE_NORMAL);
            free(deviceIdCopy); // Free the copy after use
            
            if (status == CR_SUCCESS) {
                // Try to disable the device - using proper constants
                // Use CM_DISABLE_PERSIST (0x1) instead of CM_DISABLE_PERMANENTLY
                status = CM_Disable_DevNode(devInst, 0x1); // CM_DISABLE_PERSIST
                if (status == CR_SUCCESS) {
                    std::cerr << "Device disabled successfully using CM_Disable_DevNode with persist flag" << std::endl;
                    ejectionSucceeded = true;
                } else {
                    std::cerr << "CM_Disable_DevNode failed with error: " << status << " (0x" << std::hex << status << ")" << std::endl;
                    
                    // Try without flags (0) instead of CM_DISABLE_TEMPORARY
                    status = CM_Disable_DevNode(devInst, 0);
                    if (status == CR_SUCCESS) {
                        std::cerr << "Device disabled temporarily using CM_Disable_DevNode" << std::endl;
                        ejectionSucceeded = true;
                    }
                }
            }
        }
    }
    
    // Approach 2: Use SetupDi API more thoroughly
    if (!ejectionSucceeded) {
        HDEVINFO deviceInfoSet = SetupDiGetClassDevs(
            nullptr,
            nullptr,
            nullptr,
            DIGCF_ALLCLASSES | DIGCF_PRESENT
        );
        
        if (deviceInfoSet != INVALID_HANDLE_VALUE) {
            SP_DEVINFO_DATA deviceInfoData = {0};
            deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
            
            // Find our device by instance ID
            for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); i++) {
                char buffer[MAX_PATH] = {0};
                if (SetupDiGetDeviceInstanceIdA(deviceInfoSet, &deviceInfoData, buffer, sizeof(buffer), nullptr)) {
                    // Try exact match and partial match
                    if (_stricmp(buffer, instanceId.c_str()) == 0 || 
                        strstr(buffer, instanceId.c_str()) != nullptr ||
                        strstr(instanceId.c_str(), buffer) != nullptr) {
                        
                        std::cerr << "Found matching device in SetupDi enumeration: " << buffer << std::endl;
                        
                        // Try to disable the device with standard flags
                        SP_PROPCHANGE_PARAMS params = {0};
                        params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
                        params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
                        params.StateChange = DICS_DISABLE;
                        params.Scope = DICS_FLAG_GLOBAL;
                        params.HwProfile = 0;
                        
                        if (SetupDiSetClassInstallParams(deviceInfoSet, &deviceInfoData, 
                                                       &params.ClassInstallHeader, sizeof(params)) &&
                            SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, deviceInfoSet, &deviceInfoData)) {
                            
                            std::cerr << "Device disabled successfully using SetupDi functions" << std::endl;
                            ejectionSucceeded = true;
                        } else {
                            DWORD error = GetLastError();
                            std::cerr << "SetupDi disable failed with error: " << error << " (0x" << std::hex << error << ")" << std::endl;
                            
                            // Try an alternative approach - set remove flag
                            params.StateChange = DICS_PROPCHANGE;
                            
                            if (SetupDiSetClassInstallParams(deviceInfoSet, &deviceInfoData, 
                                                          &params.ClassInstallHeader, sizeof(params)) &&
                                SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, deviceInfoSet, &deviceInfoData)) {
                                
                                std::cerr << "Device properties changed successfully" << std::endl;
                                ejectionSucceeded = true;
                            }
                        }
                        
                        // If we found a match but couldn't disable it, break anyway
                        break;
                    }
                }
            }
            
            SetupDiDestroyDeviceInfoList(deviceInfoSet);
        }
    }
    
    // Mark device as ejected in our tracking if any method succeeded
    if (ejectionSucceeded) {
        ejectedDevices.insert(deviceId);
        
        if (knownDevices.find(deviceId) != knownDevices.end()) {
            knownDevices[deviceId].isEjected = true;
        }
        
        // Display a notification to the user
        std::string deviceName = "Unknown Device";
        if (knownDevices.find(deviceId) != knownDevices.end()) {
            deviceName = knownDevices[deviceId].friendlyName;
        }
        
        std::string message = "Device has been ejected: " + deviceName;
        MessageBoxA(nullptr, message.c_str(), "Device Ejected", MB_OK | MB_ICONINFORMATION);
        
        return true;
    }
    
    std::cerr << "All device ejection methods failed" << std::endl;
    
    // Show error message to the user
    MessageBoxA(nullptr, 
               "Failed to eject device. This may require administrator privileges.\n\n"
               "Try running the application with 'run_as_admin.bat' for more permissions.",
               "Ejection Failed", MB_OK | MB_ICONERROR);
               
    return false;
}

// Method to get device instance ID from path
std::string DeviceAuthenticator::GetDeviceInstanceIdFromPath(const std::string& devicePath)
{
    // Path usually has a format like \\?\HID#VID_046D&PID_C52B#6&38c58f05&0&0000#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}
    // We want to extract the part before the #{GUID} at the end
    
    std::string path = devicePath;
    size_t guidPos = path.find("#{");
    if (guidPos != std::string::npos) {
        path = path.substr(0, guidPos);
    }
    
    // Replace # with \ which is how Windows internally represents the ID
    std::string instanceId = path;
    size_t start = instanceId.find("\\\\?\\");
    if (start != std::string::npos) {
        instanceId = instanceId.substr(start + 4); // Skip the \\?\ prefix
    }
    
    // Replace # with \ for internal Windows ID format
    for (size_t i = 0; i < instanceId.length(); i++) {
        if (instanceId[i] == '#') {
            instanceId[i] = '\\';
        }
    }
    
    return instanceId;
}

// Eject all untrusted devices
void DeviceAuthenticator::EjectUntrustedDevices()
{
    std::cerr << "Ejecting all untrusted devices..." << std::endl;
    
    int ejectedCount = 0;
    for (const auto& pair : knownDevices) {
        const std::string& deviceId = pair.first;
        const USBDeviceInfo& deviceInfo = pair.second;
        
        if (!deviceInfo.authenticated && !deviceInfo.isEjected) {
            std::cerr << "Ejecting untrusted device: " << deviceInfo.friendlyName << std::endl;
            if (EjectDevice(deviceId)) {
                ejectedCount++;
            }
        }
    }
    
    std::cerr << "Ejected " << ejectedCount << " untrusted devices" << std::endl;
}

// Eject all devices except the specified one
void DeviceAuthenticator::EjectAllExcept(const std::string& deviceId)
{
    std::cerr << "Ejecting all devices except: " << deviceId << std::endl;
    
    int ejectedCount = 0;
    for (const auto& pair : knownDevices) {
        const std::string& currentDeviceId = pair.first;
        const USBDeviceInfo& deviceInfo = pair.second;
        
        if (currentDeviceId != deviceId && !deviceInfo.isEjected) {
            std::cerr << "Ejecting device: " << deviceInfo.friendlyName << std::endl;
            if (EjectDevice(currentDeviceId)) {
                ejectedCount++;
            }
        }
    }
    
    std::cerr << "Ejected " << ejectedCount << " devices" << std::endl;
}

// Restore a previously ejected device
bool DeviceAuthenticator::RestoreDevice(const std::string& deviceId)
{
    if (ejectedDevices.find(deviceId) == ejectedDevices.end()) {
        std::cerr << "Device is not marked as ejected: " << deviceId << std::endl;
        return false;
    }
    
    std::cerr << "Attempting to restore device: " << deviceId << std::endl;
    
    std::string instanceId = GetDeviceInstanceIdFromPath(deviceId);
    if (instanceId.empty()) {
        std::cerr << "Failed to get instance ID for device: " << deviceId << std::endl;
        return false;
    }
    
    DEVINST devInst = 0;
    CONFIGRET status;
    
    // Create a non-const copy of the string (required by the API)
    char* deviceIdCopy = _strdup(instanceId.c_str());
    if (!deviceIdCopy) {
        std::cerr << "Failed to allocate memory for device ID" << std::endl;
        return false;
    }
    
    status = CM_Locate_DevNodeA(&devInst, deviceIdCopy, CM_LOCATE_DEVNODE_NORMAL);
    free(deviceIdCopy);
    
    bool restorationSucceeded = false;
    
    if (status == CR_SUCCESS) {
        // Enable the device
        status = CM_Enable_DevNode(devInst, 0);
        if (status == CR_SUCCESS) {
            std::cerr << "Device restored successfully using CM_Enable_DevNode" << std::endl;
            restorationSucceeded = true;
        } else {
            std::cerr << "CM_Enable_DevNode failed with error: " << status << std::endl;
        }
    } else {
        std::cerr << "CM_Locate_DevNodeA failed with error: " << status << std::endl;
    }
    
    // Try SetupDi methods as a fallback
    if (!restorationSucceeded) {
        HDEVINFO deviceInfoSet = SetupDiGetClassDevs(
            nullptr,
            nullptr,
            nullptr,
            DIGCF_ALLCLASSES
        );
        
        if (deviceInfoSet != INVALID_HANDLE_VALUE) {
            SP_DEVINFO_DATA deviceInfoData = {0};
            deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
            
            bool found = false;
            for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); i++) {
                char buffer[MAX_PATH] = {0};
                if (SetupDiGetDeviceInstanceIdA(deviceInfoSet, &deviceInfoData, buffer, sizeof(buffer), nullptr)) {
                    if (_stricmp(buffer, instanceId.c_str()) == 0 || 
                        strstr(buffer, instanceId.c_str()) != nullptr ||
                        strstr(instanceId.c_str(), buffer) != nullptr) {
                        
                        found = true;
                        
                        // Enable the device
                        SP_PROPCHANGE_PARAMS params = {0};
                        params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
                        params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
                        params.StateChange = DICS_ENABLE;    // Request to enable
                        params.Scope = DICS_FLAG_GLOBAL;     // Apply to all hardware profiles
                        params.HwProfile = 0;               // Current hardware profile
                        
                        if (SetupDiSetClassInstallParams(deviceInfoSet, &deviceInfoData, 
                                                       &params.ClassInstallHeader, sizeof(params)) &&
                            SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, deviceInfoSet, &deviceInfoData)) {
                            
                            std::cerr << "Device enabled successfully using SetupDi functions" << std::endl;
                            restorationSucceeded = true;
                        } else {
                            DWORD error = GetLastError();
                            std::cerr << "SetupDi enable failed with error: " << error << std::endl;
                        }
                        break;
                    }
                }
            }
            
            if (!found) {
                std::cerr << "Device not found in SetupDi enumeration" << std::endl;
            }
            
            SetupDiDestroyDeviceInfoList(deviceInfoSet);
        }
    }
    
    // Update our tracking data
    if (restorationSucceeded) {
        ejectedDevices.erase(deviceId);
        
        if (knownDevices.find(deviceId) != knownDevices.end()) {
            knownDevices[deviceId].isEjected = false;
        }
        
        return true;
    }
    
    std::cerr << "Failed to restore device" << std::endl;
    return false;
}

// Restore all previously ejected devices
void DeviceAuthenticator::RestoreAllEjectedDevices()
{
    std::cerr << "Restoring all ejected devices..." << std::endl;
    
    // Make a copy to avoid iterator invalidation during modification
    std::set<std::string> devicesCopy = ejectedDevices;
    
    int restoredCount = 0;
    for (const auto& deviceId : devicesCopy) {
        if (RestoreDevice(deviceId)) {
            restoredCount++;
        }
    }
    
    std::cerr << "Restored " << restoredCount << " ejected devices" << std::endl;
}

// Get the device instance ID from SetupAPI
std::string DeviceAuthenticator::GetDeviceInstanceId(HDEVINFO deviceInfoSet, PSP_DEVINFO_DATA deviceInfoData)
{
    char buffer[MAX_PATH] = {0};
    if (SetupDiGetDeviceInstanceIdA(deviceInfoSet, deviceInfoData, buffer, sizeof(buffer), nullptr)) {
        return std::string(buffer);
    }
    return "";
}

// Extract hardware ID (VID/PID) from device
std::string DeviceAuthenticator::GetDeviceHardwareId(HDEVINFO deviceInfoSet, PSP_DEVINFO_DATA deviceInfoData)
{
    char buffer[256] = {0};
    if (!SetupDiGetDeviceRegistryProperty(deviceInfoSet, deviceInfoData, SPDRP_HARDWAREID,
                                       nullptr, (BYTE*)buffer, sizeof(buffer), nullptr)) {
        return "";
    }
    
    std::string hwid = buffer;
    std::string vidPid;
    
    // Look for VID_ and PID_ in the hardware ID
    size_t vidPos = hwid.find("VID_");
    size_t pidPos = hwid.find("PID_");
    
    if (vidPos != std::string::npos && pidPos != std::string::npos) {
        // Extract VID and PID values (8 chars each: VID_xxxx and PID_yyyy)
        std::string vid = hwid.substr(vidPos, 8);
        std::string pid = hwid.substr(pidPos, 8);
        vidPid = vid + "&" + pid;
    }
    
    return vidPid;
}

// Extract serial number if available
std::string DeviceAuthenticator::GetDeviceSerialNumber(HDEVINFO deviceInfoSet, PSP_DEVINFO_DATA deviceInfoData)
{
    char buffer[256] = {0};
    
    // Try to get serial number from various properties
    if (SetupDiGetDeviceRegistryProperty(deviceInfoSet, deviceInfoData, SPDRP_PHYSICAL_DEVICE_OBJECT_NAME,
                                      nullptr, (BYTE*)buffer, sizeof(buffer), nullptr)) {
        return std::string(buffer);
    }
    
    return "";
}

// Check if a device matches any known trusted device
bool DeviceAuthenticator::IsKnownTrustedDevice(const std::string& deviceId)
{
    auto it = knownDevices.find(deviceId);
    if (it == knownDevices.end()) {
        return false; // New device ID, need to check hardware identifiers
    }
    
    if (it->second.authenticated) {
        return true; // Already trusted this exact device ID
    }
    
    // Get the full device info for matching
    USBDeviceInfo& newDevice = it->second;
    
    // Look for matching hardware identifiers in trusted devices
    for (const auto& pair : knownDevices) {
        const USBDeviceInfo& existingDevice = pair.second;
        
        if (existingDevice.authenticated && 
            newDevice.IsSamePhysicalDevice(existingDevice)) {
            // Found a match! This is a reconnected trusted device
            std::cerr << "Recognized reconnected trusted device: " 
                     << newDevice.friendlyName << std::endl;
                     
            // Transfer trust to this device ID
            newDevice.authenticated = true;
            return true;
        }
    }
    
    return false; // No matching trusted device found
}

// Save trusted devices to file
bool DeviceAuthenticator::SaveTrustedDevices() 
{
    try {
        std::ofstream file(trustedDevicesPath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open trusted devices file for writing" << std::endl;
            return false;
        }
        
        // Count trusted devices first
        int trustedCount = 0;
        for (const auto& pair : knownDevices) {
            if (pair.second.authenticated) {
                trustedCount++;
            }
        }
        
        // Write count
        file.write(reinterpret_cast<const char*>(&trustedCount), sizeof(int));
        
        // Write each trusted device
        for (const auto& pair : knownDevices) {
            const USBDeviceInfo& device = pair.second;
            if (device.authenticated) {
                // Write device ID
                int idLength = static_cast<int>(device.deviceId.length());
                file.write(reinterpret_cast<const char*>(&idLength), sizeof(int));
                file.write(device.deviceId.c_str(), idLength);
                
                // Write friendly name
                int nameLength = static_cast<int>(device.friendlyName.length());
                file.write(reinterpret_cast<const char*>(&nameLength), sizeof(int));
                file.write(device.friendlyName.c_str(), nameLength);
                
                // Write instance ID
                int instanceIdLength = static_cast<int>(device.instanceId.length());
                file.write(reinterpret_cast<const char*>(&instanceIdLength), sizeof(int));
                file.write(device.instanceId.c_str(), instanceIdLength);
                
                // Write hardware ID
                int hardwareIdLength = static_cast<int>(device.hardwareId.length());
                file.write(reinterpret_cast<const char*>(&hardwareIdLength), sizeof(int));
                file.write(device.hardwareId.c_str(), hardwareIdLength);
                
                // Write serial number
                int serialLength = static_cast<int>(device.serialNumber.length());
                file.write(reinterpret_cast<const char*>(&serialLength), sizeof(int));
                file.write(device.serialNumber.c_str(), serialLength);
            }
        }
        
        std::cerr << "Saved " << trustedCount << " trusted devices to " << trustedDevicesPath << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error saving trusted devices: " << e.what() << std::endl;
        return false;
    }
}

// Load trusted devices from file
bool DeviceAuthenticator::LoadTrustedDevices()
{
    try {
        std::ifstream file(trustedDevicesPath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "No trusted devices file found at " << trustedDevicesPath << std::endl;
            return false;
        }
        
        // Read count
        int trustedCount = 0;
        file.read(reinterpret_cast<char*>(&trustedCount), sizeof(int));
        
        // Read each trusted device
        for (int i = 0; i < trustedCount; i++) {
            USBDeviceInfo device;
            device.authenticated = true;
            
            // Read device ID
            int idLength = 0;
            file.read(reinterpret_cast<char*>(&idLength), sizeof(int));
            
            std::vector<char> idBuffer(idLength + 1, 0);
            file.read(idBuffer.data(), idLength);
            device.deviceId = idBuffer.data();
            
            // Read friendly name
            int nameLength = 0;
            file.read(reinterpret_cast<char*>(&nameLength), sizeof(int));
            
            std::vector<char> nameBuffer(nameLength + 1, 0);
            file.read(nameBuffer.data(), nameLength);
            device.friendlyName = nameBuffer.data();
            
            // Read instance ID
            int instanceIdLength = 0;
            file.read(reinterpret_cast<char*>(&instanceIdLength), sizeof(int));
            
            std::vector<char> instanceBuffer(instanceIdLength + 1, 0);
            file.read(instanceBuffer.data(), instanceIdLength);
            device.instanceId = instanceBuffer.data();
            
            // Read hardware ID
            int hardwareIdLength = 0;
            file.read(reinterpret_cast<char*>(&hardwareIdLength), sizeof(int));
            
            std::vector<char> hwBuffer(hardwareIdLength + 1, 0);
            file.read(hwBuffer.data(), hardwareIdLength);
            device.hardwareId = hwBuffer.data();
            
            // Read serial number
            int serialLength = 0;
            file.read(reinterpret_cast<char*>(&serialLength), sizeof(int));
            
            std::vector<char> serialBuffer(serialLength + 1, 0);
            file.read(serialBuffer.data(), serialLength);
            device.serialNumber = serialBuffer.data();
            
            // Add to known devices (using device ID as key)
            knownDevices[device.deviceId] = device;
            
            std::cerr << "Loaded trusted device: " << device.friendlyName
                     << " (HW ID: " << device.hardwareId << ")" << std::endl;
        }
        
        std::cerr << "Loaded " << trustedCount << " trusted devices" << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error loading trusted devices: " << e.what() << std::endl;
        return false;
    }
}
