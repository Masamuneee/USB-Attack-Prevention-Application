#ifndef DEVICE_AUTHENTICATOR_H
#define DEVICE_AUTHENTICATOR_H

#include <windows.h>
#include <dbt.h>         // For device notifications
#include <initguid.h>    // For DEFINE_GUID
#include <unordered_map>
#include <chrono>
#include <string>
#include <vector>
#include <set>

struct USBDeviceInfo {
    std::string deviceId;
    std::string friendlyName;
    bool authenticated;
    std::chrono::system_clock::time_point lastAuthAttempt;
    
    USBDeviceInfo() : authenticated(false) {}
    USBDeviceInfo(const std::string& id, const std::string& name) 
        : deviceId(id), friendlyName(name), authenticated(false) {}
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
};

#endif // DEVICE_AUTHENTICATOR_H
