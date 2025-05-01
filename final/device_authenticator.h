#ifndef DEVICE_AUTHENTICATOR_H
#define DEVICE_AUTHENTICATOR_H

// Windows API includes
#include <windows.h>
#include <dbt.h>         
#include <initguid.h>   // This needs to come before the DEFINE_GUID
#include <setupapi.h>
#include <devguid.h>
#include <commctrl.h>
#include <cfgmgr32.h>  // For CM_* functions
#include <fstream>    // Add for ofstream/ifstream support

// Standard library includes
#include <unordered_map>
#include <chrono>
#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <iostream>
#include <ctime>
#include <algorithm>

// Define keyboard device interface GUID
// Use extern when not defined with INITGUID to prevent multiple definitions
#ifdef INITGUID
// This is the actual definition that should appear in only one source file
DEFINE_GUID(GUID_DEVINTERFACE_KEYBOARD, 0x884b96c3, 0x56ef, 0x11d1, 0xbc, 0x8c, 0x00, 0xa0, 0xc9, 0x14, 0x05, 0xdd);
#else
// This is for all other source files - just declare it as extern
EXTERN_C const GUID DECLSPEC_SELECTANY GUID_DEVINTERFACE_KEYBOARD;
#endif

// Event types for device authentication
enum class AuthEvent {
    DEVICE_DETECTED,
    DEVICE_AUTHENTICATED,
    DEVICE_AUTH_FAILED,
    DEVICE_BLOCKED,
    DEVICE_UNTRUSTED,
    DEVICE_REMOVED
};

// Interface for objects that want to receive auth events
class IDeviceAuthListener {
public:
    virtual ~IDeviceAuthListener() = default;
    virtual void OnAuthEvent(AuthEvent event, const std::string& deviceId) = 0;
};

// Structure to store USB device information
struct USBDeviceInfo {
    std::string deviceId;
    std::string friendlyName;
    bool authenticated;
    std::chrono::system_clock::time_point lastAuthAttempt;
    bool isEjected;  // Track if the device is currently ejected
    
    USBDeviceInfo() : authenticated(false), isEjected(false) {}
    USBDeviceInfo(const std::string& id, const std::string& name) 
        : deviceId(id), friendlyName(name), authenticated(false), isEjected(false) {}
};

class DeviceAuthenticator
{
private:
    // Static hook procedure to monitor device changes
    static LRESULT CALLBACK DeviceChangeHandler(int nCode, WPARAM wParam, LPARAM lParam);
    
    // Window procedure for device notification messages
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    // Handle for the hidden window that receives device notifications
    HWND messageWindow;
    
    // Handle for the device notification
    HDEVNOTIFY deviceNotifyHandle;

    // Number of authentication attempts per device
    std::unordered_map<std::string, int> authenticationAttempts;

    // Tracks blocked devices along with the time until which they're blocked
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> blockedDevices;
    
    // List of all known USB devices
    std::unordered_map<std::string, USBDeviceInfo> knownDevices;
    
    // List of event listeners
    std::vector<IDeviceAuthListener*> listeners;

    // Tracks ejected devices that need to be restored on exit
    std::set<std::string> ejectedDevices;
    
    // Current device being authenticated
    std::string currentAuthDeviceId;

    // Attempts to authenticate a newly detected device
    bool AuthenticateDevice(const std::string& deviceId);

    // Block a device (e.g., for 1 hour) after repeated failures
    void BlockDevice(const std::string& deviceId);

    // Check if a device is currently blocked
    bool IsDeviceBlocked(const std::string& deviceId);
    
    // Register for device notifications
    void RegisterDeviceNotifications();
    
    // Enumerate existing devices
    void EnumerateExistingDevices();
    
    // Notify registered listeners of events
    void NotifyListeners(AuthEvent event, const std::string& deviceId);

    // Eject a device by disabling it
    bool EjectDevice(const std::string& deviceId);
    
    // Restore an ejected device
    bool RestoreDevice(const std::string& deviceId);
    
    // Get device instance ID from device path
    std::string GetDeviceInstanceIdFromPath(const std::string& devicePath);

    // Methods for device settings persistence
    void SaveDeviceSettings();
    void LoadDeviceSettings();

public:
    DeviceAuthenticator();
    ~DeviceAuthenticator();
    
    // Singleton pattern
    static DeviceAuthenticator& GetInstance();

    // Start listening for device-change notifications
    void Start();

    // Stop listening (unhook/unregister notifications)
    void Stop();
    
    // Get list of connected devices for UI
    std::vector<USBDeviceInfo> GetConnectedDevices() const;
    
    // Set device trust status manually
    bool SetDeviceTrust(const std::string& deviceId, bool trusted);
    
    // Untrust a previously trusted device
    bool UntrustDevice(const std::string& deviceId);
    
    // Register/unregister for auth events
    void RegisterListener(IDeviceAuthListener* listener);
    void UnregisterListener(IDeviceAuthListener* listener);

    // Eject untrusted devices
    void EjectUntrustedDevices();
    
    // Eject all devices except the one specified
    void EjectAllExcept(const std::string& deviceId);
    
    // Restore all ejected devices
    void RestoreAllEjectedDevices();
    
    // Public method to eject a specific device by ID (for UI)
    bool EjectDeviceById(const std::string& deviceId) {
        return EjectDevice(deviceId);
    }
    
    // Public method to restore a specific device by ID (for UI)
    bool RestoreDeviceById(const std::string& deviceId) {
        return RestoreDevice(deviceId);
    }
    
    // Get the device instance ID from the setup API
    static std::string GetDeviceInstanceId(HDEVINFO deviceInfoSet, PSP_DEVINFO_DATA deviceInfoData);
};

#endif // DEVICE_AUTHENTICATOR_H
