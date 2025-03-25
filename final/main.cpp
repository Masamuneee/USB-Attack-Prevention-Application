#include "key_logger.h"
#include "device_authenticator.h"
#include "behavior_analyzer.h"
#include <windows.h>

int main()
{
    KeyLogger logger;
    DeviceAuthenticator authenticator;
    BehaviorAnalyzer analyzer;

    // Possibly add blacklisted words
    analyzer.AddBlacklistedWord("cmd");
    analyzer.AddBlacklistedWord("powershell");

    logger.Start();
    authenticator.Start();
    analyzer.Start();

    // Keep the app running with a message loop so the hooks remain active
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    logger.Stop();
    authenticator.Stop();
    analyzer.Stop();

    return 0;
}
