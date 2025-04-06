#include "system_tray.h"
#include "device_authenticator.h"
#include "key_logger.h"
#include "behavior_analyzer.h"
#include <windows.h>
#include <shellapi.h>
#include <sstream>
#include <cstring>
#include <commctrl.h>
#include <vector>

// Window class and message
const char* TRAY_WINDOW_CLASS = "USBMonitorTrayClass";
const UINT WM_TRAYICON = WM_USER + 1;

// Remove all the custom ListView definitions since they're already defined in commctrl.h
// We'll use the standard Windows API definitions instead

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
                    
                case IDM_SETTINGS:
                    tray.ShowSettingsDialog();
                    return 0;
                    
                case IDM_ABOUT:
                    tray.ShowAboutDialog();
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
    // Try to load the icon from the executable directory
    std::string iconPath = GetExecutableDirectory() + "\\usb_icon.ico";
    HICON hIcon = (HICON)LoadImage(
        nullptr,
        iconPath.c_str(),
        IMAGE_ICON,
        16, 16,
        LR_LOADFROMFILE
    );
    
    // Fallback to the default icon if custom icon not found
    if (!hIcon) {
        hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    }
    
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = hIcon;
    
    // Use safe string copy
    strncpy(nid.szTip, "USB Device Monitor (v1.0.3)", sizeof(nid.szTip) - 1);
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
    // Update menu - we'll rebuild it each time to ensure it's up to date
    if (popupMenu) {
        DestroyMenu(popupMenu);
    }
    
    popupMenu = CreatePopupMenu();
    AppendMenu(popupMenu, MF_STRING, IDM_SHOW_DEVICES, "Manage USB Devices");
    AppendMenu(popupMenu, MF_STRING, IDM_SETTINGS, "Settings");
    AppendMenu(popupMenu, MF_STRING | (isLoggingEnabled ? MF_CHECKED : MF_UNCHECKED), IDM_TOGGLE_LOGGING, "Enable Logging");
    AppendMenu(popupMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(popupMenu, MF_STRING, IDM_ABOUT, "About");
    AppendMenu(popupMenu, MF_STRING, IDM_EXIT, "Exit");
    
    // Make sure the window is the foreground window
    SetForegroundWindow(hwnd);
    
    // Show the menu
    POINT pt;
    GetCursorPos(&pt);
    
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
        "USB Device Manager",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        100, 100, 500, 400,
        hwnd, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    // Create informational text
    CreateWindowEx(
        0, "STATIC", 
        "Connected USB devices - Use the buttons below to manage device trust status:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 10, 480, 20, 
        hDlg, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    // Create a listview control
    HWND hListView = CreateWindowEx(
        0, WC_LISTVIEW, "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_BORDER | WS_TABSTOP,
        10, 40, 480, 280,
        hDlg, (HMENU)IDC_DEVICE_LIST, GetModuleHandle(nullptr), nullptr
    );
    
    // Add columns - using standard Windows API
    LV_COLUMN lvc = {0};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    
    lvc.iSubItem = 0;
    lvc.cx = 230;
    lvc.pszText = (LPSTR)"Device Name";
    ListView_InsertColumn(hListView, 0, &lvc);
    
    lvc.iSubItem = 1;
    lvc.cx = 80;
    lvc.pszText = (LPSTR)"Status";
    ListView_InsertColumn(hListView, 1, &lvc);
    
    lvc.iSubItem = 2;
    lvc.cx = 150;
    lvc.pszText = (LPSTR)"Device ID";
    ListView_InsertColumn(hListView, 2, &lvc);
    
    // Add items - using standard Windows API
    LV_ITEM lvi = {0};
    lvi.mask = LVIF_TEXT | LVIF_PARAM;
    
    for (size_t i = 0; i < devices.size(); i++) {
        lvi.iItem = (int)i;
        lvi.lParam = (LPARAM)i;  // Store index for reference
        
        // Device name
        lvi.iSubItem = 0;
        lvi.pszText = (LPSTR)devices[i].friendlyName.c_str();
        ListView_InsertItem(hListView, &lvi);
        
        // Status
        lvi.iSubItem = 1;
        lvi.pszText = (LPSTR)(devices[i].authenticated ? "Trusted" : "Not Trusted");
        ListView_SetItem(hListView, &lvi);
        
        // Device ID (shortened for display)
        lvi.iSubItem = 2;
        std::string shortId = devices[i].deviceId;
        if (shortId.length() > 20) {
            shortId = shortId.substr(0, 17) + "...";
        }
        lvi.pszText = (LPSTR)shortId.c_str();
        ListView_SetItem(hListView, &lvi);
    }
    
    // Store device list for button callbacks
    SetProp(hDlg, "DeviceList", (HANDLE)&devices);
    
    // Add buttons
    HWND hBtnTrust = CreateWindow(
        "BUTTON", "Trust Device",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        10, 330, 100, 25,
        hDlg, (HMENU)IDM_TRUST_DEVICE, GetModuleHandle(nullptr), nullptr
    );
    
    HWND hBtnUntrust = CreateWindow(
        "BUTTON", "Untrust Device",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        120, 330, 100, 25,
        hDlg, (HMENU)IDM_UNTRUST_DEVICE, GetModuleHandle(nullptr), nullptr
    );
    
    HWND hBtnDetails = CreateWindow(
        "BUTTON", "View Details",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        230, 330, 100, 25,
        hDlg, (HMENU)IDM_DEVICE_DETAILS, GetModuleHandle(nullptr), nullptr
    );
    
    HWND hBtnClose = CreateWindow(
        "BUTTON", "Close",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        390, 330, 100, 25,
        hDlg, (HMENU)IDOK, GetModuleHandle(nullptr), nullptr
    );
    
    // Create a status bar
    CreateStatusWindow(
        WS_CHILD | WS_VISIBLE,
        "Select a device to view or modify its trust settings",
        hDlg, IDC_STATUS_BAR
    );
    
    // Device selection handler
    SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)this);
    
    // Store dialog procedure in a static function to avoid lambda conversion issues
    struct DialogProcHelper {
        static LRESULT CALLBACK DeviceListProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
            SystemTray* pThis = (SystemTray*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            
            switch (msg) {
                case WM_COMMAND:
                    if (LOWORD(wParam) == IDOK) {
                        DestroyWindow(hwnd);
                        return TRUE;
                    } 
                    else if (LOWORD(wParam) == IDM_TRUST_DEVICE || 
                             LOWORD(wParam) == IDM_UNTRUST_DEVICE || 
                             LOWORD(wParam) == IDM_DEVICE_DETAILS) {
                        
                        HWND hList = GetDlgItem(hwnd, IDC_DEVICE_LIST);
                        int selectedIndex = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
                        
                        if (selectedIndex >= 0) {
                            auto& devices = *(std::vector<USBDeviceInfo>*)GetProp(hwnd, "DeviceList");
                            DeviceAuthenticator& auth = DeviceAuthenticator::GetInstance();
                            
                            if (LOWORD(wParam) == IDM_TRUST_DEVICE) {
                                if (auth.SetDeviceTrust(devices[selectedIndex].deviceId, true)) {
                                    // Update the status in the list
                                    LV_ITEM lvi = {0};
                                    lvi.mask = LVIF_TEXT;
                                    lvi.iItem = selectedIndex;
                                    lvi.iSubItem = 1;
                                    lvi.pszText = (LPSTR)"Trusted";
                                    ListView_SetItem(hList, &lvi);
                                    devices[selectedIndex].authenticated = true;
                                }
                            }
                            else if (LOWORD(wParam) == IDM_UNTRUST_DEVICE) {
                                if (auth.UntrustDevice(devices[selectedIndex].deviceId)) {
                                    // Update the status in the list
                                    LV_ITEM lvi = {0};
                                    lvi.mask = LVIF_TEXT;
                                    lvi.iItem = selectedIndex;
                                    lvi.iSubItem = 1;
                                    lvi.pszText = (LPSTR)"Not Trusted";
                                    ListView_SetItem(hList, &lvi);
                                    devices[selectedIndex].authenticated = false;
                                }
                            }
                            else if (LOWORD(wParam) == IDM_DEVICE_DETAILS) {
                                std::string message = "Device ID: " + devices[selectedIndex].deviceId + 
                                                     "\nFriendly Name: " + devices[selectedIndex].friendlyName +
                                                     "\nStatus: " + (devices[selectedIndex].authenticated ? "Trusted" : "Not Trusted");
                                                     
                                MessageBoxA(hwnd, message.c_str(), "Device Details", MB_OK | MB_ICONINFORMATION);
                            }
                        }
                        else {
                            MessageBoxA(hwnd, "Please select a device first", "No Selection", MB_OK | MB_ICONINFORMATION);
                        }
                        return TRUE;
                    }
                    break;
                    
                case WM_NOTIFY: {
                    NMHDR* nmhdr = (NMHDR*)lParam;
                    if (nmhdr->code == LVN_ITEMCHANGED && nmhdr->idFrom == IDC_DEVICE_LIST) {
                        NMLISTVIEW* nmlv = (NMLISTVIEW*)lParam;
                        if (nmlv->uNewState & LVIS_SELECTED) {
                            auto& devices = *(std::vector<USBDeviceInfo>*)GetProp(hwnd, "DeviceList");
                            int selectedIndex = nmlv->iItem;
                            
                            // Update status bar with selected device info
                            if (selectedIndex >= 0 && selectedIndex < (int)devices.size()) {
                                std::string statusText = "Selected device: " + devices[selectedIndex].friendlyName;
                                SetWindowTextA(GetDlgItem(hwnd, IDC_STATUS_BAR), statusText.c_str());
                            }
                        }
                    }
                    break;
                }
                    
                case WM_DESTROY:
                    RemoveProp(hwnd, "DeviceList");
                    break;
            }
            
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }
    };
    
    // Set the window procedure using the static method
    SetWindowLongPtr(hDlg, GWLP_WNDPROC, (LONG_PTR)DialogProcHelper::DeviceListProc);
    
    // Show and bring the dialog to front
    ShowWindow(hDlg, SW_SHOW);
    SetForegroundWindow(hDlg);
    UpdateWindow(hDlg);
    
    // Modal message loop will be handled by the main application loop
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

void SystemTray::ShowSettingsDialog()
{
    HWND hDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        "STATIC",
        "USB Monitor Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        100, 100, 400, 300,
        hwnd, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    // Create general settings section
    HWND hGeneralGroup = CreateWindowEx(
        0, "BUTTON", "General Settings",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, 10, 380, 120,
        hDlg, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    // Create checkboxes for settings
    HWND hCbLogging = CreateWindowEx(
        0, "BUTTON", "Enable Keypress Logging",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        20, 30, 200, 20,
        hDlg, (HMENU)IDC_CB_ENABLE_LOGGING, GetModuleHandle(nullptr), nullptr
    );
    
    HWND hCbBlockInput = CreateWindowEx(
        0, "BUTTON", "Block Suspicious Input",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        20, 55, 200, 20,
        hDlg, (HMENU)IDC_CB_BLOCK_SUSPICIOUS, GetModuleHandle(nullptr), nullptr
    );
    
    HWND hCbStartWithWindows = CreateWindowEx(
        0, "BUTTON", "Start with Windows",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        20, 80, 200, 20,
        hDlg, (HMENU)IDC_CB_STARTUP, GetModuleHandle(nullptr), nullptr
    );
    
    // Blacklist section
    HWND hBlacklistGroup = CreateWindowEx(
        0, "BUTTON", "Blacklisted Keywords",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, 140, 380, 110,
        hDlg, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    CreateWindowEx(
        0, "STATIC", "Add keyword to blacklist:",
        WS_CHILD | WS_VISIBLE,
        20, 160, 150, 20,
        hDlg, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    HWND hEditKeyword = CreateWindowEx(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        170, 160, 130, 20,
        hDlg, (HMENU)IDC_EDIT_KEYWORD, GetModuleHandle(nullptr), nullptr
    );
    
    HWND hBtnAdd = CreateWindowEx(
        0, "BUTTON", "Add",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        310, 160, 70, 20,
        hDlg, (HMENU)IDC_BTN_ADD_KEYWORD, GetModuleHandle(nullptr), nullptr
    );
    
    // Show current blacklisted words
    CreateWindowEx(
        0, "STATIC", "Current blacklist:",
        WS_CHILD | WS_VISIBLE,
        20, 185, 100, 20,
        hDlg, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    HWND hListBlacklist = CreateWindowEx(
        WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS,
        20, 205, 280, 40,
        hDlg, (HMENU)IDC_LIST_BLACKLIST, GetModuleHandle(nullptr), nullptr
    );
    
    HWND hBtnRemove = CreateWindowEx(
        0, "BUTTON", "Remove",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        310, 205, 70, 20,
        hDlg, (HMENU)IDC_BTN_REMOVE_KEYWORD, GetModuleHandle(nullptr), nullptr
    );
    
    // Add OK and Cancel buttons
    HWND hBtnOK = CreateWindowEx(
        0, "BUTTON", "OK",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        230, 265, 70, 25,
        hDlg, (HMENU)IDOK, GetModuleHandle(nullptr), nullptr
    );
    
    HWND hBtnCancel = CreateWindowEx(
        0, "BUTTON", "Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        310, 265, 70, 25,
        hDlg, (HMENU)IDCANCEL, GetModuleHandle(nullptr), nullptr
    );
    
    // Set initial checkbox states
    SendMessage(hCbLogging, BM_SETCHECK, isLoggingEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(hCbBlockInput, BM_SETCHECK, BehaviorAnalyzer::GetInstance().GetBlockSuspiciousInput() ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(hCbStartWithWindows, BM_SETCHECK, IsStartupEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);
    
    // Populate blacklist
    BehaviorAnalyzer& analyzer = BehaviorAnalyzer::GetInstance();
    
    // NOTE: We don't have direct access to the blacklisted words in the analyzer
    // This is a placeholder - you would need to add a method to BehaviorAnalyzer
    // to retrieve the current list of blacklisted words
    // For now, add the default ones we know about
    SendMessageA(hListBlacklist, LB_ADDSTRING, 0, (LPARAM)"cmd");
    SendMessageA(hListBlacklist, LB_ADDSTRING, 0, (LPARAM)"powershell");
    
    // Set up dialog procedure using static function instead of lambda
    struct DialogProcHelper {
        static LRESULT CALLBACK SettingsDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
            SystemTray* pThis = (SystemTray*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            
            switch (msg) {
                case WM_COMMAND:
                    if (LOWORD(wParam) == IDOK) {
                        // Save settings
                        bool enableLogging = (SendDlgItemMessage(hwnd, IDC_CB_ENABLE_LOGGING, BM_GETCHECK, 0, 0) == BST_CHECKED);
                        bool blockSuspicious = (SendDlgItemMessage(hwnd, IDC_CB_BLOCK_SUSPICIOUS, BM_GETCHECK, 0, 0) == BST_CHECKED);
                        bool startWithWindows = (SendDlgItemMessage(hwnd, IDC_CB_STARTUP, BM_GETCHECK, 0, 0) == BST_CHECKED);
                        
                        // Apply settings
                        pThis->EnableLogging(enableLogging);
                        BehaviorAnalyzer::GetInstance().SetBlockSuspiciousInput(blockSuspicious);
                        pThis->SetStartupEnabled(startWithWindows);
                        
                        DestroyWindow(hwnd);
                        return TRUE;
                    } 
                    else if (LOWORD(wParam) == IDCANCEL) {
                        DestroyWindow(hwnd);
                        return TRUE;
                    }
                    else if (LOWORD(wParam) == IDC_BTN_ADD_KEYWORD) {
                        char keyword[100] = {0};
                        GetDlgItemTextA(hwnd, IDC_EDIT_KEYWORD, keyword, 100);
                        
                        if (keyword[0] != '\0') {
                            // Add to list box
                            SendDlgItemMessageA(hwnd, IDC_LIST_BLACKLIST, LB_ADDSTRING, 0, (LPARAM)keyword);
                            
                            // Add to analyzer
                            BehaviorAnalyzer::GetInstance().AddBlacklistedWord(keyword);
                            
                            // Clear input field
                            SetDlgItemTextA(hwnd, IDC_EDIT_KEYWORD, "");
                        }
                        return TRUE;
                    }
                    else if (LOWORD(wParam) == IDC_BTN_REMOVE_KEYWORD) {
                        HWND hList = GetDlgItem(hwnd, IDC_LIST_BLACKLIST);
                        int sel = SendMessage(hList, LB_GETCURSEL, 0, 0);
                        
                        if (sel != LB_ERR) {
                            char word[100] = {0};
                            SendMessageA(hList, LB_GETTEXT, sel, (LPARAM)word);
                            
                            // Remove from list box
                            SendMessage(hList, LB_DELETESTRING, sel, 0);
                            
                            // NOTE: We would need to add a RemoveBlacklistedWord method to BehaviorAnalyzer
                            // This is a placeholder
                            // BehaviorAnalyzer::GetInstance().RemoveBlacklistedWord(word);
                        }
                        return TRUE;
                    }
                    break;
                    
                case WM_DESTROY:
                    break;
            }
            
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }
    };
    
    // Set the window procedure using the static method
    SetWindowLongPtr(hDlg, GWLP_WNDPROC, (LONG_PTR)DialogProcHelper::SettingsDialogProc);
    
    // Show and center dialog
    ShowWindow(hDlg, SW_SHOW);
    SetForegroundWindow(hDlg);
    UpdateWindow(hDlg);
}

bool SystemTray::IsStartupEnabled()
{
    // Check if the app is set to run at startup
    HKEY hKey;
    bool enabled = false;
    
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char value[MAX_PATH] = {0};
        DWORD size = sizeof(value);
        
        if (RegQueryValueExA(hKey, "USBMonitor", nullptr, nullptr, (BYTE*)value, &size) == ERROR_SUCCESS) {
            enabled = true;
        }
        
        RegCloseKey(hKey);
    }
    
    return enabled;
}

void SystemTray::SetStartupEnabled(bool enable)
{
    HKEY hKey;
    
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 
                      0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        
        if (enable) {
            // Get the executable path
            char path[MAX_PATH];
            GetModuleFileNameA(nullptr, path, MAX_PATH);
            
            // Set the registry value
            RegSetValueExA(hKey, "USBMonitor", 0, REG_SZ, (BYTE*)path, strlen(path) + 1);
        } else {
            // Remove the registry value
            RegDeleteValueA(hKey, "USBMonitor");
        }
        
        RegCloseKey(hKey);
    }
}

void SystemTray::ShowAboutDialog()
{
    MessageBoxA(
        hwnd,
        "USB Attack Prevention Application\n"
        "Version 1.0.3\n\n"
        "A comprehensive security solution for protecting against malicious USB devices.\n\n"
        "Authors: Masamune (Minh Pham) / Lio (Thai Do)",
        "About USB Monitor",
        MB_OK | MB_ICONINFORMATION
    );
}

// Helper function to get executable directory
std::string SystemTray::GetExecutableDirectory()
{
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    
    std::string fullPath(path);
    size_t pos = fullPath.find_last_of("\\/");
    
    if (pos != std::string::npos) {
        return fullPath.substr(0, pos);
    }
    
    return "";
}
