#ifndef DEVICE_AUTHENTICATOR_H
#define DEVICE_AUTHENTICATOR_H

#include <windows.h>
#include <unordered_map>
#include <chrono>
#include <string>

class DeviceAuthenticator
{
private:
    // Static hook procedure to monitor device changes (for demo only)
    static LRESULT CALLBACK DeviceChangeHandler(int nCode, WPARAM wParam, LPARAM lParam);

    // Number of authentication attempts per device
    std::unordered_map<std::string, int> authenticationAttempts;

    // Tracks blocked devices along with the time until which they're blocked
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> blockedDevices;

    // Attempts to authenticate a newly detected device
    bool AuthenticateDevice(const std::string& deviceId);

    // Block a device (e.g., for 1 hour) after repeated failures
    void BlockDevice(const std::string& deviceId);

    // Check if a device is currently blocked
    bool IsDeviceBlocked(const std::string& deviceId);

public:
    DeviceAuthenticator();
    ~DeviceAuthenticator();

    // Start listening for device-change notifications
    void Start();

    // Stop listening (unhook/unregister notifications)
    void Stop();
};

#endif // DEVICE_AUTHENTICATOR_H
