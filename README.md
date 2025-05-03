# USB Attack Prevention Application

**Version:** 1.0.6
**Author:** Masamune (Minh Pham) / Lio (Thai Do)

## Description
A comprehensive security solution for protecting against malicious USB devices through multi-layered defense mechanisms. This application helps safeguard your system by monitoring and controlling USB keyboard devices, filtering suspicious input, and preventing automated attacks.

## Features

- **Key Logging and Monitoring**: Records all keyboard activity with detailed timing and pattern analysis
- **Device Authentication**: Forces new USB devices to verify through a challenge-response mechanism
- **Behavior Analysis**: Detects suspicious keystroke patterns and blocks potentially harmful commands
- **System Tray Integration**: Easy access to device management and application controls
- **Device Trust Management**: Ability to trust and untrust devices through an intuitive UI
- **Enhanced Device Ejection**: Automatically eject untrusted devices with detailed device identification
- **Settings Configuration**: Customize application behavior and security preferences
- **Startup Integration**: Option to launch automatically when Windows starts
- **Raw Input API**: Enhanced keyboard monitoring with advanced device detection

## Prerequisites

- Microsoft Windows (tested on Windows 10/11)
- Microsoft Visual Studio 2019+ or MinGW/GCC compiler
- Windows API and common controls libraries

## Building the Project

### Using the Makefile
The easiest way to build the project is using the included Makefile:

```
make
```

### Using the compile.bat script
For Windows users without make installed, use the compile.bat script:

```
compile.bat
```

### Using administrative privileges
For device ejection functionality, you can run with administrative privileges:

```
StartApp.bat
```

### Manual compilation
You can also compile manually with:

```
g++ final/key_logger.cpp final/device_authenticator.cpp final/behavior_analyzer.cpp final/system_tray.cpp final/main.cpp -o usb_hooks.exe -mwindows -std=c++17 -lsetupapi -lcomctl32 -lcfgmgr32
```

## Project Structure

- **key_logger.cpp/.h**: Monitors and logs all keyboard activities
- **device_authenticator.cpp/.h**: Handles USB device detection and authentication
- **behavior_analyzer.cpp/.h**: Analyzes keystroke patterns and filters suspicious input
- **system_tray.cpp/.h**: Provides user interface through system tray integration
- **resource.h**: Contains ID definitions for UI elements
- **main.cpp**: Application entry point and initialization

## Usage

1. Run `usb_hooks.exe` to start the application
2. The application will appear in the system tray (near the clock)
3. Right-click on the tray icon to access options:
   - **Manage USB Devices:** View, trust, or untrust connected USB devices
   - **Settings:** Configure application behavior and security preferences
   - **Enable/Disable Logging:** Toggle keystroke logging
   - **About:** View application information
   - **Exit:** Close the application

## Security Layers

### Layer 1: Device Authentication
When a new USB keyboard device is connected, the application will:
- Prompt for authentication using a random 4-character code
- Block devices after 5 failed authentication attempts (for 1 hour)
- Remember trusted devices for future sessions
- Allow manual trusting/untrusting of devices through the UI
- Automatically eject untrusted devices for enhanced security

### Layer 2: Behavior Analysis
Even after authentication, the application monitors for:
- Unusually fast typing speeds (potential automation)
- Blacklisted commands or patterns (customizable through Settings)
- Suspicious key sequence patterns
- Optional automatic blocking of suspicious input

### Layer 3: Activity Logging
All keyboard events are logged with:
- Timestamps
- Key sequences
- Timing intervals
- Alerts for suspicious activity

## Contributing

### Development Workflow
1. Fork this repository
2. Create a feature branch: `git checkout -b feature/your-feature-name`
3. Commit your changes: `git commit -m 'Add some feature'`
4. Push to your fork: `git push origin feature/your-feature-name`
5. Create a pull request

## Troubleshooting

### Common Issues
- **Compilation errors**: Make sure all required libraries are installed
- **"Permission denied"**: Ensure the application isn't running when recompiling
- **Device not detected**: Verify you have administrative privileges
- **UI not displaying**: Check that comctl32.lib is properly linked
- **Device ejection fails**: Run the application with administrative privileges using StartApp.bat

## License
This project is licensed under the [MIT License](LICENSE) - see the LICENSE file for details.

## Acknowledgments
- Windows API documentation
- SetupAPI and Device Management API
- Common Controls library
- Configuration Manager API for device ejection functionality

