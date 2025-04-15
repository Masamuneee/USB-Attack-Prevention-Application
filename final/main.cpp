// Include device_authenticator.h first to ensure GUID is defined once
#include "device_authenticator.h"
#include "key_logger.h"
#include "behavior_analyzer.h"
#include "system_tray.h"
#include <windows.h>
#include <string.h>

// Previous main function converted to init function
void InitializeApplication()
{
    // Get references to singletons
    KeyLogger& logger = KeyLogger::GetInstance();
    DeviceAuthenticator& authenticator = DeviceAuthenticator::GetInstance();
    BehaviorAnalyzer& analyzer = BehaviorAnalyzer::GetInstance();

    // Add blacklisted words
    analyzer.AddBlacklistedWord("cmd");
    analyzer.AddBlacklistedWord("powershell");

    // Start services
    logger.Start();
    authenticator.Start();
    analyzer.Start();
}

// Windows GUI application entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Check for test mode (used by CI/CD)
    if (lpCmdLine && strstr(lpCmdLine, "--test-mode") != nullptr) {
        // In test mode, just initialize and exit immediately
        return 0;
    }

    // Initialize everything
    InitializeApplication();
    
    // Initialize and run the system tray
    SystemTray& tray = SystemTray::GetInstance();
    if (tray.Initialize()) {
        tray.RunMessageLoop();
    }

    // Cleanup happens automatically in destructors
    return 0;
}
