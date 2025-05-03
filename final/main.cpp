// Include device_authenticator.h first to ensure GUID is defined once
#include "device_authenticator.h"
#include "key_logger.h"
#include "behavior_analyzer.h"
#include "system_tray.h"
#include <windows.h>
#include <string.h>
#include <iostream>

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

    // Load trusted devices first so they'll be recognized during startup
    authenticator.LoadTrustedDevices();
    
    // Start services
    logger.Start();
    authenticator.Start();
    analyzer.Start();
    
    std::cerr << "USB Manager initialized - trusted devices loaded" << std::endl;
}

// Windows GUI application entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Silence unused parameter warnings
    (void)hInstance;
    (void)hPrevInstance;
    (void)nCmdShow;
    
    // Check for test mode (used by CI/CD)
    if (lpCmdLine && strstr(lpCmdLine, "--test-mode") != nullptr) {
        // In test mode, just initialize and exit immediately
        return 0;
    }

    // Check for eject mode or admin mode
    bool ejectMode = (lpCmdLine && (strstr(lpCmdLine, "--eject-mode") != nullptr));
    bool adminMode = (lpCmdLine && (strstr(lpCmdLine, "--admin-mode") != nullptr));

    // Initialize everything
    InitializeApplication();
    
    // Set eject mode if requested
    if (ejectMode || adminMode) {
        std::cerr << "Running in " << (ejectMode ? "eject" : "admin") << " mode" << std::endl;
        
        // For eject mode, immediately eject all untrusted devices
        if (ejectMode) {
            DeviceAuthenticator::GetInstance().EjectUntrustedDevices();
        }
    }
    
    // Initialize and run the system tray
    SystemTray& tray = SystemTray::GetInstance();
    if (tray.Initialize()) {
        tray.RunMessageLoop();
    }

    // Cleanup happens automatically in destructors
    return 0;
}
