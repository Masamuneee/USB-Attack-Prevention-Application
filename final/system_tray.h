#ifndef SYSTEM_TRAY_H
#define SYSTEM_TRAY_H

#include "resource.h" // For menu IDs and resource identifiers
#include "key_logger.h"
#include "device_authenticator.h"  // Already has the GUID definition
#include "behavior_analyzer.h"
#include <commctrl.h>
#include <shellapi.h>
#include <windows.h>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

// Add common controls and listview message definitions
#ifndef LVM_SETEXTENDEDLISTVIEWSTYLE
#define LVM_SETEXTENDEDLISTVIEWSTYLE (LVM_FIRST + 54)
#endif

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
    void CenterWindowOnScreen(HWND hWnd);

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
