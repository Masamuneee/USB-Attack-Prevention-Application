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

// Enhanced prompt for authentication
static bool PromptUserForAuthentication(HWND parent, const std::string& code)
{
    // Instead of using DialogBoxParam with a resource, we'll create a dialog dynamically
    // or use MessageBox as a fallback
    
    std::ostringstream oss;
    oss << "A new keyboard device has been detected.\n"
        << "Please type the following code on that keyboard to verify: "
        << code << "\n"
        << "Press OK if correct, or Cancel if incorrect.";

    int result = MessageBoxA(parent, oss.str().c_str(), "Device Authentication", MB_OKCANCEL | MB_ICONINFORMATION);
    return (result == IDOK);
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

    // Add to known devices if not already there
    if (knownDevices.find(deviceId) == knownDevices.end()) {
        knownDevices[deviceId] = USBDeviceInfo(deviceId, "Unknown Device");
    }
    
    // Update last authentication attempt time
    knownDevices[deviceId].lastAuthAttempt = std::chrono::system_clock::now();

    // Prompt user
    std::string code = GenerateChallengeCode();
    bool userOk = PromptUserForAuthentication(messageWindow, code);
    if (userOk) {
        knownDevices[deviceId].authenticated = true;
        return true;
    } else {
        authenticationAttempts[deviceId]++;
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
