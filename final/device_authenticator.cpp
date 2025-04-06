#include "device_authenticator.h"
#include <dbt.h> // For DBT_DEVICEARRIVAL, etc.
#include <sstream>
#include <iostream>
#include <ctime>
#include <setupapi.h>
#include <initguid.h>
#include <devguid.h>
#include <commctrl.h>
#include <algorithm>

// Microsoft Visual C++ uses pragma comment
#ifdef _MSC_VER
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "comctl32.lib")
#endif

// For MinGW, you need to add -lsetupapi when compiling

// Define keyboard device interface GUID if not already defined
#ifndef GUID_DEVINTERFACE_KEYBOARD
// HID keyboard GUID
DEFINE_GUID(GUID_DEVINTERFACE_KEYBOARD, 0x884b96c3, 0x56ef, 0x11d1, 0xbc, 0x8c, 0x00, 0xa0, 0xc9, 0x14, 0x05, 0xdd);
#endif

// Define a custom dialog resource ID since we don't have a resource file
#define IDD_DIALOG_AUTH 1001

// Window class name for device notification handling
const char* DEVICE_WINDOW_CLASS = "USBDeviceNotificationClass";
// Message when a device has been authenticated
const UINT WM_DEVICE_AUTHENTICATED = WM_USER + 100;

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

// Get friendly name for a device
static std::string GetDeviceFriendlyName(HDEVINFO deviceInfoSet, PSP_DEVINFO_DATA deviceInfoData)
{
    char buffer[256] = {0};
    DWORD bufferSize = sizeof(buffer);
    
    if (SetupDiGetDeviceRegistryProperty(deviceInfoSet, deviceInfoData, 
                                         SPDRP_FRIENDLYNAME, nullptr, 
                                         (BYTE*)buffer, bufferSize, nullptr)) {
        return std::string(buffer);
    }
    
    // Try device description if friendly name isn't available
    if (SetupDiGetDeviceRegistryProperty(deviceInfoSet, deviceInfoData, 
                                         SPDRP_DEVICEDESC, nullptr, 
                                         (BYTE*)buffer, bufferSize, nullptr)) {
        return std::string(buffer);
    }
    
    return "Unknown Device";
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
    : messageWindow(nullptr), deviceNotifyHandle(nullptr)
{
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
            
            USBDeviceInfo deviceInfo(deviceId, friendlyName);
            knownDevices[deviceId] = deviceInfo;
            
            // Prompt to trust existing devices
            std::string message = "Do you trust this keyboard device?\n" + friendlyName;
            int result = MessageBoxA(nullptr, message.c_str(), "Device Trust", MB_YESNO | MB_ICONQUESTION);
            
            knownDevices[deviceId].authenticated = (result == IDYES);
        }
    }
    
    SetupDiDestroyDeviceInfoList(deviceInfoSet);
}

bool DeviceAuthenticator::AuthenticateDevice(const std::string& deviceId)
{
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
    
    // Update last authentication attempt time
    knownDevices[deviceId].lastAuthAttempt = std::chrono::system_clock::now();

    // Generate challenge code and prompt user
    std::string code = GenerateChallengeCode();
    bool userOk = PromptUserForAuthentication(messageWindow, deviceName, code);
    
    if (userOk) {
        knownDevices[deviceId].authenticated = true;
        
        // Notify listeners about the successful authentication
        NotifyListeners(AuthEvent::DEVICE_AUTHENTICATED, deviceId);
        return true;
    } else {
        authenticationAttempts[deviceId]++;
        
        // Notify listeners about the failed authentication
        NotifyListeners(AuthEvent::DEVICE_AUTH_FAILED, deviceId);
        return false;
    }
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
