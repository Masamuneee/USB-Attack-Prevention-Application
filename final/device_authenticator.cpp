#include "device_authenticator.h"
#include <dbt.h> // For DBT_DEVICEARRIVAL, etc.
#include <sstream>
#include <iostream>
#include <ctime>

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

// Simulate a prompt for demonstration purposes. In reality, use a proper GUI.
static bool PromptUserForAuthentication(const std::string& code)
{
    std::ostringstream oss;
    oss << "A new keyboard device has been detected.\n"
        << "Please type the following code on that keyboard to verify: "
        << code << "\n"
        << "Press OK if correct, or Cancel if incorrect.";

    int response = MessageBoxA(nullptr, oss.str().c_str(), "Device Authentication", MB_OKCANCEL | MB_ICONINFORMATION);
    return (response == IDOK); // if user clicked OK, we consider it successful
}

// Static callback for device changes. Typically you'd use RegisterDeviceNotification or so.
LRESULT CALLBACK DeviceAuthenticator::DeviceChangeHandler(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION) {
        if (wParam == DBT_DEVICEARRIVAL) {
            // Gather device information from lParam if needed. We'll just mock an ID here.
            std::string deviceId = "keyboard_" + std::to_string(rand());

            DeviceAuthenticator auth;
            if (!auth.IsDeviceBlocked(deviceId)) {
                bool success = auth.AuthenticateDevice(deviceId);
                if (!success) {
                    auth.BlockDevice(deviceId);
                }
            } else {
                // Device is blocked, optionally log or notify user
                std::cerr << "[Authenticator] Device is currently blocked: " << deviceId << "\n";
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

DeviceAuthenticator::DeviceAuthenticator()
{
}

DeviceAuthenticator::~DeviceAuthenticator()
{
    Stop();
}

void DeviceAuthenticator::Start()
{
    // Typically we'd do device notification registration. For a demo, a CBT or shell hook can be used.
    // In real usage, consider RegisterDeviceNotification for device interface notifications.
    SetWindowsHookEx(WH_CBT, DeviceChangeHandler, GetModuleHandle(nullptr), 0);
}

void DeviceAuthenticator::Stop()
{
    // Unhook if you stored the hook handle. For demonstration, it's not stored in a member variable.
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

    // Prompt user
    std::string code = GenerateChallengeCode();
    bool userOk = PromptUserForAuthentication(code);
    if (userOk) {
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
