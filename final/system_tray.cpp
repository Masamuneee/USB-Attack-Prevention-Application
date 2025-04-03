#include "system_tray.h"
#include "device_authenticator.h"
#include "key_logger.h"
#include "behavior_analyzer.h"
#include <windows.h>
#include <shellapi.h>
#include <sstream>
#include <cstring>

// Window class and message
const char* TRAY_WINDOW_CLASS = "USBMonitorTrayClass";
const UINT WM_TRAYICON = WM_USER + 1;

// For list view control support
#ifndef LVS_REPORT
#define LVS_REPORT 0x0001
#endif
#ifndef LVS_SINGLESEL
#define LVS_SINGLESEL 0x0004
#endif
#ifndef LVCF_TEXT
#define LVCF_TEXT 0x0001
#endif
#ifndef LVCF_WIDTH
#define LVCF_WIDTH 0x0002
#endif
#ifndef LVCF_SUBITEM
#define LVCF_SUBITEM 0x0008
#endif
#ifndef LVIF_TEXT
#define LVIF_TEXT 0x0001
#endif

// Define ListView messages if not already defined
#ifndef LVM_FIRST
#define LVM_FIRST 0x1000
#endif
#ifndef LVM_INSERTCOLUMN
#define LVM_INSERTCOLUMN (LVM_FIRST + 27)
#endif
#ifndef LVM_INSERTITEM
#define LVM_INSERTITEM (LVM_FIRST + 7)
#endif
#ifndef LVM_SETITEM
#define LVM_SETITEM (LVM_FIRST + 6)
#endif

// Define list view control if needed
#ifndef WC_LISTVIEW
#define WC_LISTVIEWA "SysListView32"
#define WC_LISTVIEW WC_LISTVIEWA
#endif

// ListView message macros
#define ListView_InsertColumn(hwnd, iCol, pcol) \
    (int)SendMessage((hwnd), LVM_INSERTCOLUMN, (WPARAM)(iCol), (LPARAM)(pcol))
#define ListView_InsertItem(hwnd, pitem) \
    (int)SendMessage((hwnd), LVM_INSERTITEM, 0, (LPARAM)(pitem))
#define ListView_SetItem(hwnd, pitem) \
    (BOOL)SendMessage((hwnd), LVM_SETITEM, 0, (LPARAM)(pitem))

// ListView structures
typedef struct {
    UINT mask;
    int fmt;
    int cx;
    LPSTR pszText;
    int cchTextMax;
    int iSubItem;
} LVCOLUMN, *LPLVCOLUMN;

typedef struct {
    UINT mask;
    int iItem;
    int iSubItem;
    UINT state;
    UINT stateMask;
    LPSTR pszText;
    int cchTextMax;
    int iImage;
    LPARAM lParam;
} LVITEM, *LPLVITEM;

// Singleton implementation
SystemTray& SystemTray::GetInstance()
{
    static SystemTray instance;
    return instance;
}

LRESULT CALLBACK SystemTray::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SystemTray& tray = SystemTray::GetInstance();
    
    switch (uMsg) {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
                tray.ShowContextMenu();
            }
            return 0;
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDM_EXIT:
                    tray.Exit();
                    PostQuitMessage(0);
                    return 0;
                    
                case IDM_SHOW_DEVICES:
                    tray.ShowDeviceList();
                    return 0;
                    
                case IDM_TOGGLE_LOGGING:
                    tray.EnableLogging(!tray.isLoggingEnabled);
                    return 0;
            }
            break;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

SystemTray::SystemTray()
    : hwnd(nullptr), popupMenu(nullptr), isLoggingEnabled(true)
{
    ZeroMemory(&nid, sizeof(nid));
}

SystemTray::~SystemTray()
{
    DeleteTrayIcon();
    
    if (popupMenu) {
        DestroyMenu(popupMenu);
    }
    
    if (hwnd) {
        DestroyWindow(hwnd);
        UnregisterClass(TRAY_WINDOW_CLASS, GetModuleHandle(nullptr));
    }
}

bool SystemTray::Initialize()
{
    // Skip common controls initialization as it's causing issues
    
    // Register the window class
    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = WindowProc;
    wcex.hInstance = GetModuleHandle(nullptr);
    wcex.lpszClassName = TRAY_WINDOW_CLASS;
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    
    if (!RegisterClassEx(&wcex)) {
        return false;
    }
    
    // Create the window
    hwnd = CreateWindowEx(
        0,
        TRAY_WINDOW_CLASS,
        "USB Monitor",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        400, 300,
        nullptr, nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );
    
    if (!hwnd) {
        return false;
    }
    
    // Create the context menu
    popupMenu = CreatePopupMenu();
    AppendMenu(popupMenu, MF_STRING, IDM_SHOW_DEVICES, "Show USB Devices");
    AppendMenu(popupMenu, MF_STRING | (isLoggingEnabled ? MF_CHECKED : MF_UNCHECKED), IDM_TOGGLE_LOGGING, "Enable Logging");
    AppendMenu(popupMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(popupMenu, MF_STRING, IDM_EXIT, "Exit");
    
    // Create the tray icon
    CreateTrayIcon();
    
    return true;
}

void SystemTray::CreateTrayIcon()
{
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION); // Replace with a custom icon
    
    // Use safe string copy
    strncpy(nid.szTip, "USB Device Monitor", sizeof(nid.szTip) - 1);
    nid.szTip[sizeof(nid.szTip) - 1] = '\0'; // Ensure null-termination
    
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void SystemTray::DeleteTrayIcon()
{
    if (nid.cbSize) {
        Shell_NotifyIcon(NIM_DELETE, &nid);
    }
}

void SystemTray::ShowContextMenu()
{
    POINT pt;
    GetCursorPos(&pt);
    
    // Update menu check state
    CheckMenuItem(popupMenu, IDM_TOGGLE_LOGGING, 
                 MF_BYCOMMAND | (isLoggingEnabled ? MF_CHECKED : MF_UNCHECKED));
    
    // Make sure the window is the foreground window
    SetForegroundWindow(hwnd);
    
    // Show the menu
    TrackPopupMenu(
        popupMenu,
        TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
        pt.x, pt.y,
        0,
        hwnd,
        nullptr
    );
    
    // Required by Windows
    PostMessage(hwnd, WM_NULL, 0, 0);
}

void SystemTray::ShowDeviceList()
{
    // Get the list of connected devices
    std::vector<USBDeviceInfo> devices = DeviceAuthenticator::GetInstance().GetConnectedDevices();
    
    if (devices.empty()) {
        MessageBoxA(hwnd, "No USB devices found.", "Device List", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    // Create a dialog to display devices
    HWND hDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        "STATIC",
        "USB Devices",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        100, 100, 400, 300,
        hwnd, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    // Create a listview control
    HWND hListView = CreateWindowEx(
        0, WC_LISTVIEW, "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        10, 10, 380, 240,
        hDlg, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    // Add columns
    LVCOLUMN lvc = {0};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    
    lvc.iSubItem = 0;
    lvc.cx = 200;
    lvc.pszText = (LPSTR)"Device Name";
    ListView_InsertColumn(hListView, 0, &lvc);
    
    lvc.iSubItem = 1;
    lvc.cx = 80;
    lvc.pszText = (LPSTR)"Status";
    ListView_InsertColumn(hListView, 1, &lvc);
    
    // Add items
    LVITEM lvi = {0};
    lvi.mask = LVIF_TEXT;
    
    for (size_t i = 0; i < devices.size(); i++) {
        lvi.iItem = (int)i;
        
        // Device name
        lvi.iSubItem = 0;
        lvi.pszText = (LPSTR)devices[i].friendlyName.c_str();
        ListView_InsertItem(hListView, &lvi);
        
        // Status
        lvi.iSubItem = 1;
        lvi.pszText = (LPSTR)(devices[i].authenticated ? "Trusted" : "Not Trusted");
        ListView_SetItem(hListView, &lvi);
    }
    
    // Add a close button
    HWND hBtnClose = CreateWindow(
        "BUTTON", "Close",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        310, 260, 80, 25,
        hDlg, (HMENU)IDOK, GetModuleHandle(nullptr), nullptr
    );
    
    // Show the dialog
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
    
    // Modal message loop
    MSG msg;
    BOOL bRet;
    while ((bRet = GetMessage(&msg, nullptr, 0, 0)) != 0) {
        if (bRet == -1) {
            // Error occurred
            break;
        }
        
        if (msg.message == WM_COMMAND && LOWORD(msg.wParam) == IDOK) {
            DestroyWindow(hDlg);
            break;
        }
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void SystemTray::RunMessageLoop()
{
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void SystemTray::Exit()
{
    KeyLogger::GetInstance().Stop();
    DeviceAuthenticator::GetInstance().Stop();
    BehaviorAnalyzer::GetInstance().Stop();
    
    DeleteTrayIcon();
    DestroyWindow(hwnd);
}

void SystemTray::EnableLogging(bool enabled)
{
    isLoggingEnabled = enabled;
    
    if (enabled) {
        KeyLogger::GetInstance().Start();
    } else {
        KeyLogger::GetInstance().Stop();
    }
}
