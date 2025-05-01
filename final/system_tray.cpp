#include "system_tray.h"

// Window class and message
const char* TRAY_WINDOW_CLASS = "USBMonitorTrayClass";
const UINT WM_TRAYICON = WM_USER + 1;

// Add missing ListView and shell notification constants
#ifndef LVS_EX_FULLROWSELECT
#define LVS_EX_FULLROWSELECT 0x00000020
#endif

#ifndef LVS_EX_GRIDLINES
#define LVS_EX_GRIDLINES 0x00000001
#endif

// Shell version detection for different NOTIFYICONDATA sizes
#ifndef NOTIFYICON_VERSION
#define NOTIFYICON_VERSION 3
#endif

#ifndef NIF_INFO
#define NIF_INFO 0x00000010
#endif

#ifndef NIIF_INFO
#define NIIF_INFO 0x00000001
#endif

// Define ListView_SetExtendedListViewStyle macro if not defined
#ifndef ListView_SetExtendedListViewStyle
#define ListView_SetExtendedListViewStyle(hwndLV, dw) \
            (DWORD)SendMessage((hwndLV), LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)(dw))
#endif

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
            Shell_NotifyIcon(NIM_MODIFY, &tray.nid);
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
                case IDM_EJECT_UNTRUSTED:
                    DeviceAuthenticator::GetInstance().EjectUntrustedDevices();
                    MessageBoxA(hwnd, "Untrusted USB devices have been ejected.", "Device Ejection", MB_OK | MB_ICONINFORMATION);
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
    // Load the icon from resources
    HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_TRAY_ICON));
    
    // If resource loading failed, try to load from file
    if (!hIcon) {
        // Try to load the icon from the executable directory
        std::string iconPath = GetExecutableDirectory() + "\\usb_application.ico";
        hIcon = (HICON)LoadImage(
            nullptr,
            iconPath.c_str(),
            IMAGE_ICON,
            16, 16,
            LR_LOADFROMFILE
        );
        
        // If custom icon failed, use system icons
        if (!hIcon) {
            // Use standard system icons instead of IDI_SHIELD (which might not be available)
            hIcon = LoadIcon(NULL, IDI_APPLICATION);
            if (!hIcon) {
                hIcon = LoadIcon(NULL, IDI_EXCLAMATION);
                if (!hIcon) {
                    hIcon = LoadIcon(NULL, IDI_WINLOGO);
                }
            }
        }
    }
    
    // Initialize the NOTIFYICONDATA structure
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; // Remove NIF_INFO for compatibility
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = hIcon;
    
    // Use safe string copy
    #ifdef _MSC_VER
    strncpy_s(nid.szTip, sizeof(nid.szTip), "USB Device Monitor (v1.0.4)", sizeof(nid.szTip) - 1);
    #else
    strncpy(nid.szTip, "USB Device Monitor (v1.0.4)", sizeof(nid.szTip) - 1);
    #endif
    nid.szTip[sizeof(nid.szTip) - 1] = '\0'; // Ensure null-termination
    
    // Add the icon to the system tray - use a simpler approach to avoid version issues
    if (!Shell_NotifyIcon(NIM_ADD, &nid)) {
        // If adding failed, try with fewer features
        nid.uFlags = NIF_ICON | NIF_MESSAGE;
        Shell_NotifyIcon(NIM_ADD, &nid);
    }
    
    // Set up a periodic timer to ensure the icon stays visible
    SetTimer(hwnd, 1, 5000, nullptr);  // Timer ID 1, every 5 seconds
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
    AppendMenu(popupMenu, MF_STRING, IDM_EJECT_UNTRUSTED, "Eject Untrusted Devices");  // New option
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

    // Create a dialog to display devices with improved styling
    HWND hDlg = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE,
        "STATIC",
        "USB Device Manager",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | WS_THICKFRAME,
        100, 100, 750, 500,  // Larger size for better visibility
        hwnd, nullptr, GetModuleHandle(nullptr), nullptr
    );

    // Create informational text with better styling
    HWND hInfoText = CreateWindowEx(
        0, "STATIC", 
        "Connected USB devices - Use the buttons below to manage device trust status:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        15, 15, 720, 20,
        hDlg, nullptr, GetModuleHandle(nullptr), nullptr
    );

    // Use a better font for the UI text
    HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    if (hFont) {
        SendMessage(hInfoText, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    
    // Create a listview control with better styling
    HWND hListView = CreateWindowEx(
        WS_EX_CLIENTEDGE, 
        WC_LISTVIEW, "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL | WS_BORDER | WS_TABSTOP,
        15, 40, 720, 350,
        hDlg, (HMENU)IDC_DEVICE_LIST, GetModuleHandle(nullptr), nullptr
    );

    // Enable full row selection and gridlines for better visibility
    SendMessage(hListView, LVM_SETEXTENDEDLISTVIEWSTYLE, 
                LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES, 
                LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    
    // Add columns - using standard Windows API with improved widths
    LV_COLUMN lvc = {0};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    lvc.iSubItem = 0;
    lvc.cx = 240;  // Wider for better readability
    lvc.pszText = (LPSTR)"Device Name";
    ListView_InsertColumn(hListView, 0, &lvc);
    
    lvc.iSubItem = 1;
    lvc.cx = 100;
    lvc.pszText = (LPSTR)"Status";
    ListView_InsertColumn(hListView, 1, &lvc);
    
    lvc.iSubItem = 2;
    lvc.cx = 90;
    lvc.pszText = (LPSTR)"Ejected";
    ListView_InsertColumn(hListView, 2, &lvc);
    
    lvc.iSubItem = 3;
    lvc.cx = 270;
    lvc.pszText = (LPSTR)"Device ID";
    ListView_InsertColumn(hListView, 3, &lvc);

    // Apply the font to the ListView
    if (hFont) {
        SendMessage(hListView, WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    // Add items - using standard Windows API with improved display
    for (size_t i = 0; i < devices.size(); i++) {
        LV_ITEM lvi = {0};
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = (int)i;
        lvi.lParam = (LPARAM)i;  // Store index for reference

        // Create a uniquely identifiable name for devices with the same name
        std::string displayName = devices[i].friendlyName;
        if (displayName.empty()) {
            displayName = "Unknown Device";
        }
        
        int nameCount = 0;
        for (size_t j = 0; j < i; j++) {
            if (devices[j].friendlyName == devices[i].friendlyName) {
                nameCount++;
            }
        }
        
        if (nameCount > 0) {
            displayName += " (" + std::to_string(nameCount + 1) + ")";
        }

        // Device name
        lvi.iSubItem = 0;
        lvi.pszText = (LPSTR)displayName.c_str();
        ListView_InsertItem(hListView, &lvi);

        // Status
        lvi.iSubItem = 1;
        std::string status = devices[i].authenticated ? "Trusted" : "Not Trusted";
        lvi.pszText = (LPSTR)status.c_str();
        ListView_SetItem(hListView, &lvi);
        
        // Ejected status
        lvi.iSubItem = 2;
        std::string ejectedStatus = devices[i].isEjected ? "Yes" : "No";
        lvi.pszText = (LPSTR)ejectedStatus.c_str();
        ListView_SetItem(hListView, &lvi);

        // Device ID - show a shortened version
        lvi.iSubItem = 3;
        std::string shortId = devices[i].deviceId;
        if (shortId.length() > 40) {
            shortId = shortId.substr(0, 37) + "...";
        }
        lvi.pszText = (LPSTR)shortId.c_str();
        ListView_SetItem(hListView, &lvi);
    }

    // After adding all devices to the list, sort them by name for better usability
    ListView_SortItems(hListView, [](LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) -> int {
        std::vector<USBDeviceInfo>* devices = reinterpret_cast<std::vector<USBDeviceInfo>*>(lParamSort);
        const USBDeviceInfo& device1 = (*devices)[lParam1];
        const USBDeviceInfo& device2 = (*devices)[lParam2];
        return _stricmp(device1.friendlyName.c_str(), device2.friendlyName.c_str());
    }, (LPARAM)&devices);

    // Store device list for button callbacks
    SetProp(hDlg, "DeviceList", (HANDLE)&devices);

    // Add buttons with improved styling and additional button for ejection
    HWND hBtnTrust = CreateWindow(
        "BUTTON", "Trust Device",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        15, 410, 120, 30,
        hDlg, (HMENU)IDM_TRUST_DEVICE, GetModuleHandle(nullptr), nullptr
    );
    
    HWND hBtnUntrust = CreateWindow(
        "BUTTON", "Untrust Device",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        145, 410, 120, 30,
        hDlg, (HMENU)IDM_UNTRUST_DEVICE, GetModuleHandle(nullptr), nullptr
    );

    HWND hBtnEject = CreateWindow(
        "BUTTON", "Eject Device",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        275, 410, 120, 30,
        hDlg, (HMENU)IDM_EJECT_DEVICE, GetModuleHandle(nullptr), nullptr
    );

    HWND hBtnDetails = CreateWindow(
        "BUTTON", "View Details",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        405, 410, 120, 30,
        hDlg, (HMENU)IDM_DEVICE_DETAILS, GetModuleHandle(nullptr), nullptr
    );
    
    HWND hBtnClose = CreateWindow(
        "BUTTON", "Close",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        615, 410, 120, 30,
        hDlg, (HMENU)IDOK, GetModuleHandle(nullptr), nullptr
    );
    
    // Apply font to buttons
    if (hFont) {
        SendMessage(hBtnTrust, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hBtnUntrust, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hBtnEject, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hBtnDetails, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hBtnClose, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    
    // Create a status bar with improved styling
    HWND hStatusBar = CreateStatusWindow(
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        "Select a device to view or modify its trust settings",
        hDlg, IDC_STATUS_BAR
    );
    if (hFont) {
        SendMessage(hStatusBar, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    
    // Device selection handler
    SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)this);

    // Create a separate copy of device list for the dialog procedure
    std::vector<USBDeviceInfo>* deviceListCopy = new std::vector<USBDeviceInfo>(devices);
    SetProp(hDlg, "DeviceListData", deviceListCopy);
    
    // Store dialog procedure in a static function to avoid lambda conversion issues
    struct DialogProcHelper {
        static LRESULT CALLBACK DeviceListProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
            SystemTray* pThis = (SystemTray*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            
            switch (msg) {
                case WM_COMMAND:
                    if (LOWORD(wParam) == IDOK) {
                        // Clean up device list copy before destroying window
                        std::vector<USBDeviceInfo>* deviceListCopy = 
                            (std::vector<USBDeviceInfo>*)GetProp(hwnd, "DeviceListData");
                        if (deviceListCopy) {
                            delete deviceListCopy;
                        }
                        
                        RemoveProp(hwnd, "DeviceListData");
                        DestroyWindow(hwnd);
                        return TRUE;
                    }
                    else if (LOWORD(wParam) == IDM_TRUST_DEVICE || 
                             LOWORD(wParam) == IDM_UNTRUST_DEVICE || 
                             LOWORD(wParam) == IDM_EJECT_DEVICE ||
                             LOWORD(wParam) == IDM_DEVICE_DETAILS) {
                        
                        HWND hList = GetDlgItem(hwnd, IDC_DEVICE_LIST);
                        int selectedIndex = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
                        
                        if (selectedIndex >= 0) {
                            std::vector<USBDeviceInfo>* deviceListCopy = 
                                (std::vector<USBDeviceInfo>*)GetProp(hwnd, "DeviceListData");
                            
                            if (!deviceListCopy || selectedIndex >= (int)deviceListCopy->size()) {
                                MessageBoxA(hwnd, "Error accessing device data. Please try again.", 
                                          "Error", MB_OK | MB_ICONERROR);
                                return TRUE;
                            }
                            
                            DeviceAuthenticator& auth = DeviceAuthenticator::GetInstance();
                            std::string deviceId = (*deviceListCopy)[selectedIndex].deviceId;
                            
                            if (LOWORD(wParam) == IDM_TRUST_DEVICE) {
                                // Show confirmation with device details
                                std::string message = "Are you sure you want to trust this device?\n\n";
                                message += "Device Name: " + (*deviceListCopy)[selectedIndex].friendlyName + "\n";
                                message += "Device ID: " + deviceId + "\n\n";
                                message += "Trusting this device allows it to send keystrokes to your system.";
                                
                                int result = MessageBoxA(hwnd, message.c_str(), "Trust Device?", 
                                                       MB_YESNO | MB_ICONQUESTION);
                                
                                if (result == IDYES && auth.SetDeviceTrust(deviceId, true)) {
                                    // Update the status in the list
                                    (*deviceListCopy)[selectedIndex].authenticated = true;
                                    // Update UI
                                    LV_ITEM lvi = {0};
                                    lvi.mask = LVIF_TEXT;
                                    lvi.iItem = selectedIndex;
                                    lvi.iSubItem = 1;
                                    lvi.pszText = (LPSTR)"Trusted";
                                    ListView_SetItem(hList, &lvi);
                                    SetWindowTextA(GetDlgItem(hwnd, IDC_STATUS_BAR), 
                                                "Device has been trusted successfully");
                                } else if (result == IDYES) {
                                    MessageBoxA(hwnd, "Failed to trust device. The device may no longer be connected.", 
                                              "Trust Failed", MB_OK | MB_ICONERROR);
                                }
                            }
                            else if (LOWORD(wParam) == IDM_UNTRUST_DEVICE) {
                                // Show confirmation with device details
                                std::string message = "Are you sure you want to untrust this device?\n\n";
                                message += "Device Name: " + (*deviceListCopy)[selectedIndex].friendlyName + "\n";
                                message += "Device ID: " + deviceId + "\n\n";
                                message += "Untrusting this device will prevent it from sending keystrokes to your system.";
                                
                                int result = MessageBoxA(hwnd, message.c_str(), "Untrust Device?", 
                                                       MB_YESNO | MB_ICONQUESTION);
                                
                                if (result == IDYES && auth.UntrustDevice(deviceId)) {
                                    // Update the status in the list
                                    (*deviceListCopy)[selectedIndex].authenticated = false;
                                    // Update UI
                                    LV_ITEM lvi = {0};
                                    lvi.mask = LVIF_TEXT;
                                    lvi.iItem = selectedIndex;
                                    lvi.iSubItem = 1;
                                    lvi.pszText = (LPSTR)"Not Trusted";
                                    ListView_SetItem(hList, &lvi);
                                    SetWindowTextA(GetDlgItem(hwnd, IDC_STATUS_BAR), 
                                                "Device has been untrusted successfully");
                                } else if (result == IDYES) {
                                    MessageBoxA(hwnd, "Failed to untrust device. The device may no longer be connected.", 
                                              "Untrust Failed", MB_OK | MB_ICONERROR);
                                }
                            }
                            else if (LOWORD(wParam) == IDM_EJECT_DEVICE) {
                                // Show confirmation
                                std::string message = "Are you sure you want to eject this device?\n\n";
                                message += "Device Name: " + (*deviceListCopy)[selectedIndex].friendlyName + "\n";
                                message += "Device ID: " + deviceId + "\n\n";
                                message += "This will disable the device until the application is restarted or you manually restore it.";
                                
                                int result = MessageBoxA(hwnd, message.c_str(), "Eject Device?", 
                                                       MB_YESNO | MB_ICONQUESTION);
                                
                                if (result == IDYES) {
                                    if (auth.EjectDeviceById(deviceId)) {
                                        // Update UI to show ejected status
                                        (*deviceListCopy)[selectedIndex].isEjected = true;
                                        
                                        // Refresh the entire list to show accurate information
                                        ListView_DeleteAllItems(hList);
                                        
                                        // Refresh the list with current device information
                                        std::vector<USBDeviceInfo> updatedDevices = auth.GetConnectedDevices();
                                        
                                        // Update our copy
                                        *deviceListCopy = updatedDevices;
                                        
                                        // Repopulate the list
                                        for (size_t i = 0; i < updatedDevices.size(); i++) {
                                            LV_ITEM lvi = {0};
                                            lvi.mask = LVIF_TEXT | LVIF_PARAM;
                                            lvi.iItem = (int)i;
                                            lvi.lParam = (LPARAM)i;
                                            
                                            // Device name
                                            std::string displayName = updatedDevices[i].friendlyName;
                                            lvi.iSubItem = 0;
                                            lvi.pszText = (LPSTR)displayName.c_str();
                                            ListView_InsertItem(hList, &lvi);
                                            
                                            // Status
                                            lvi.iSubItem = 1;
                                            std::string status = updatedDevices[i].authenticated ? "Trusted" : "Not Trusted";
                                            lvi.pszText = (LPSTR)status.c_str();
                                            ListView_SetItem(hList, &lvi);
                                            
                                            // Ejected status
                                            lvi.iSubItem = 2;
                                            std::string ejectedStatus = updatedDevices[i].isEjected ? "Yes" : "No";
                                            lvi.pszText = (LPSTR)ejectedStatus.c_str();
                                            ListView_SetItem(hList, &lvi);
                                            
                                            // Device ID
                                            lvi.iSubItem = 3;
                                            std::string shortId = updatedDevices[i].deviceId;
                                            if (shortId.length() > 40) {
                                                shortId = shortId.substr(0, 37) + "...";
                                            }
                                            lvi.pszText = (LPSTR)shortId.c_str();
                                            ListView_SetItem(hList, &lvi);
                                        }
                                        
                                        // Reselect the device if it's still in the list
                                        for (size_t i = 0; i < updatedDevices.size(); i++) {
                                            if (updatedDevices[i].deviceId == deviceId) {
                                                ListView_SetItemState(hList, i, LVIS_SELECTED, LVIS_SELECTED);
                                                break;
                                            }
                                        }
                                        
                                        SetWindowTextA(GetDlgItem(hwnd, IDC_STATUS_BAR), 
                                                    "Device has been ejected successfully");
                                    } else {
                                        MessageBoxA(hwnd, 
                                                  "Failed to eject device. The operation might require administrator privileges.\n\n"
                                                  "Try running the application as administrator, or the device might be protected by the system.",
                                                  "Eject Failed", MB_OK | MB_ICONERROR);
                                    }
                                }
                            }
                            else if (LOWORD(wParam) == IDM_DEVICE_DETAILS) {
                                const USBDeviceInfo& device = (*deviceListCopy)[selectedIndex];
                                
                                std::string message = "Device Details:\n\n";
                                message += "Name: " + device.friendlyName + "\n";
                                message += "Status: " + std::string(device.authenticated ? "Trusted" : "Not Trusted") + "\n";
                                message += "Ejected: " + std::string(device.isEjected ? "Yes" : "No") + "\n";
                                message += "Device ID: " + device.deviceId + "\n";
                                
                                // Add last authentication attempt time if available
                                if (device.lastAuthAttempt.time_since_epoch().count() > 0) {
                                    std::time_t authTime = std::chrono::system_clock::to_time_t(device.lastAuthAttempt);
                                    std::tm authTm = {};
                                    
                                    #ifdef _MSC_VER
                                        localtime_s(&authTm, &authTime);
                                    #else
                                        std::tm* temp_tm = localtime(&authTime);
                                        if (temp_tm) {
                                            authTm = *temp_tm;
                                        }
                                    #endif
                                    
                                    char timeBuf[64] = {0};
                                    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &authTm);
                                    
                                    message += "Last Authentication: " + std::string(timeBuf) + "\n";
                                }
                                
                                MessageBoxA(hwnd, message.c_str(), "Device Details", MB_OK | MB_ICONINFORMATION);
                            }
                        } else {
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
                            std::vector<USBDeviceInfo>* deviceListCopy = 
                                (std::vector<USBDeviceInfo>*)GetProp(hwnd, "DeviceListData");
                            
                            if (deviceListCopy) {
                                int selectedIndex = nmlv->iItem;
                                if (selectedIndex >= 0 && selectedIndex < (int)deviceListCopy->size()) {
                                    const USBDeviceInfo& device = (*deviceListCopy)[selectedIndex];
                                    
                                    // Format status bar text with more info
                                    std::string statusText = "Selected: " + device.friendlyName + 
                                                           " | Status: " + (device.authenticated ? "Trusted" : "Not Trusted") +
                                                           " | Ejected: " + (device.isEjected ? "Yes" : "No");
                                    SetWindowTextA(GetDlgItem(hwnd, IDC_STATUS_BAR), statusText.c_str());
                                }
                            }
                        }
                    }
                    break;
                }
                
                case WM_DESTROY:
                    // Clean up if not already done
                    std::vector<USBDeviceInfo>* deviceListCopy = 
                        (std::vector<USBDeviceInfo>*)GetProp(hwnd, "DeviceListData");
                    
                    if (deviceListCopy) {
                        delete deviceListCopy;
                        RemoveProp(hwnd, "DeviceListData");
                    }
                    
                    // Delete font
                    HFONT hFont = (HFONT)GetProp(hwnd, "DialogFont");
                    if (hFont) {
                        DeleteObject(hFont);
                        RemoveProp(hwnd, "DialogFont");
                    }
                    break;
            }
            
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }
    };
    // Set the window procedure using the static method
    SetWindowLongPtr(hDlg, GWLP_WNDPROC, (LONG_PTR)DialogProcHelper::DeviceListProc);
    
    // Store the font for cleanup
    SetProp(hDlg, "DialogFont", hFont);
    
    // Show and bring the dialog to front
    ShowWindow(hDlg, SW_SHOW);
    SetForegroundWindow(hDlg);
    UpdateWindow(hDlg);
    
    // Center the dialog on the screen
    RECT rcDlg, rcDesktop;
    GetWindowRect(hDlg, &rcDlg);
    GetWindowRect(GetDesktopWindow(), &rcDesktop);
    
    int dlgWidth = rcDlg.right - rcDlg.left;
    int dlgHeight = rcDlg.bottom - rcDlg.top;
    
    int newX = (rcDesktop.right - dlgWidth) / 2;
    int newY = (rcDesktop.bottom - dlgHeight) / 2;
    
    SetWindowPos(hDlg, nullptr, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
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
    SendMessageA(hListBlacklist, LB_ADDSTRING, 0, (LPARAM)"cmd");
    SendMessageA(hListBlacklist, LB_ADDSTRING, 0, (LPARAM)"powershell");    
    
    // Set up dialog procedure using static function instead of lambda
    struct DialogProcHelper {
        static LRESULT CALLBACK SettingsDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
            SystemTray* pThis = (SystemTray*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            
            switch (msg) {
                case WM_COMMAND:
                    if (LOWORD(wParam) == IDOK) {
                        bool enableLogging = (SendDlgItemMessage(hwnd, IDC_CB_ENABLE_LOGGING, BM_GETCHECK, 0, 0) == BST_CHECKED);
                        bool blockSuspicious = (SendDlgItemMessage(hwnd, IDC_CB_BLOCK_SUSPICIOUS, BM_GETCHECK, 0, 0) == BST_CHECKED);
                        bool startWithWindows = (SendDlgItemMessage(hwnd, IDC_CB_STARTUP, BM_GETCHECK, 0, 0) == BST_CHECKED);
                        
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
                            SendDlgItemMessageA(hwnd, IDC_LIST_BLACKLIST, LB_ADDSTRING, 0, (LPARAM)keyword);
                            BehaviorAnalyzer::GetInstance().AddBlacklistedWord(keyword);
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
                            SendMessage(hList, LB_DELETESTRING, sel, 0);
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
    SetWindowLongPtr(hDlg, GWLP_WNDPROC, (LONG_PTR)DialogProcHelper::SettingsDialogProc);
    ShowWindow(hDlg, SW_SHOW);
    SetForegroundWindow(hDlg);
    UpdateWindow(hDlg);
}

bool SystemTray::IsStartupEnabled()
{
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
            char path[MAX_PATH];
            GetModuleFileNameA(nullptr, path, MAX_PATH);
            DWORD pathLen = static_cast<DWORD>(strlen(path) + 1); // Explicit cast
            RegSetValueExA(hKey, "USBMonitor", 0, REG_SZ, (BYTE*)path, pathLen);
        } else {
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
        "Version 1.0.4\n\n"
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
