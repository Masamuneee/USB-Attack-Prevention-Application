 # USB Attack Prevention Application

**Version:** 1.0.2
**Author:** Masamune (Minh Pham) / Lio (Thai Do)

## Description
A comprehensive security solution for protecting against malicious USB devices through multi-layered defense mechanisms.

## Features

- **Key Logging and Monitoring**: Records all keyboard activity with detailed timing and pattern analysis
- **Device Authentication**: Forces new USB devices to verify through a challenge-response mechanism
- **Behavior Analysis**: Detects suspicious keystroke patterns and blocks potentially harmful commands
- **System Tray Integration**: Easy access to device management and application controls

## Prerequisites

- Microsoft Windows (tested on Windows 10/11).
- Microsoft Visual Studio or any other compiler that supports Windows API.
- MinGW/GCC compiler for building from source.

## Building the Project

Use the included Makefile to build the project:

```
make
```

Or compile manually with:

```
g++ key_logger.cpp device_authenticator.cpp behavior_analyzer.cpp system_tray.cpp main.cpp -o usb_hooks.exe -mwindows -std=c++17 -lsetupapi
```

## Usage

1. Run `usb_hooks.exe` to start the application
2. The application will appear in the system tray
3. Right-click on the tray icon to access options:
   - **Show USB Devices:** View and manage connected USB devices
   - **Enable/Disable Logging:** Toggle keystroke logging
   - **Exit:** Close the application

## Security Layers

### Layer 1: Device Authentication
When a new USB keyboard device is connected, the application will:
- Prompt for authentication using a random 4-character code
- Block devices after 5 failed authentication attempts
- Remember trusted devices for future sessions

### Layer 2: Behavior Analysis
Even after authentication, the application monitors for:
- Unusually fast typing speeds (potential automation)
- Blacklisted commands or patterns
- Suspicious key sequence patterns

### Layer 3: Activity Logging
All keyboard events are logged with:
- Timestamps
- Key sequences
- Timing intervals
- Alerts for suspicious activity

## Contributing
- Fork this repository.
- Create a new branch for your feature.
- Submit a pull request with a clear description.

## License
This project is licensed under the [MIT License](LICENSE) - see the LICENSE file for details.

