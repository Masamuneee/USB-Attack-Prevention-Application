#ifndef SYSTEM_TRAY_H
#define SYSTEM_TRAY_H

#include <windows.h>
#include <string>
#include "resource.h" // For menu IDs

// Menu command IDs are now in resource.h

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
    void ShowDeviceList();

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
