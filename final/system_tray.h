#ifndef SYSTEM_TRAY_H
#define SYSTEM_TRAY_H

#include <windows.h>
#include <string>
#include "resource.h" // For menu IDs

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
