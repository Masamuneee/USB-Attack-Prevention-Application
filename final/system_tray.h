#ifndef SYSTEM_TRAY_H
#define SYSTEM_TRAY_H

#include "resource.h" // For menu IDs
#include "device_authenticator.h"
#include "key_logger.h"
#include "behavior_analyzer.h"
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <string>
#include <sstream>
#include <cstring>
#include <vector>

// Menu and control IDs are now in resource.h

class SystemTray
{
private:
    HWND hwnd;
    NOTIFYICONDATA nid;
    HMENU popupMenu;
    bool isLoggingEnabled;
    
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void CreateTrayIcon();
    void DeleteTrayIcon();
    void ShowContextMenu();
    
    // UI functions
    void ShowDeviceList();
    void ShowSettingsDialog();
    void ShowAboutDialog();
    
    // Helper methods
    std::string GetExecutableDirectory();
    bool IsStartupEnabled();
    void SetStartupEnabled(bool enable);

public:
    SystemTray();
    ~SystemTray();
    
    // Singleton pattern
    static SystemTray& GetInstance();
    
    bool Initialize();
    void RunMessageLoop();
    void Exit();
    
    // Toggle logging state
    void EnableLogging(bool enabled);
};

#endif // SYSTEM_TRAY_H
