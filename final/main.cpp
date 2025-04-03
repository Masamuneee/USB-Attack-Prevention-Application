#include "key_logger.h"
#include "device_authenticator.h"
#include "behavior_analyzer.h"
#include "system_tray.h"
#include <windows.h>

int main()
{
    // Get references to singletons
    KeyLogger& logger = KeyLogger::GetInstance();
    DeviceAuthenticator& authenticator = DeviceAuthenticator::GetInstance();
    BehaviorAnalyzer& analyzer = BehaviorAnalyzer::GetInstance();
    SystemTray& tray = SystemTray::GetInstance();

    // Add blacklisted words
    analyzer.AddBlacklistedWord("cmd");
    analyzer.AddBlacklistedWord("powershell");

    // Start services
    logger.Start();
    authenticator.Start();
    analyzer.Start();
    
    // Initialize and run the system tray
    if (tray.Initialize()) {
        tray.RunMessageLoop();
    }

    // Cleanup happens automatically in destructors
    return 0;
}
