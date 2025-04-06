#include "key_logger.h"
#include "device_authenticator.h"
#include "behavior_analyzer.h"
#include "system_tray.h"
#include <windows.h>

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
