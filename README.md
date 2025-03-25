 # USB Attack Prevention Application

**Version:** 1.0.1 
**Author:** Masamune (Minh Pham) / Lio (Thai Do)

## Description
A C++ application to detect and prevent potential USB-based keystroke injection attacks. It:
1. Monitors keystrokes from the keyboard in real-time.
2. Detects newly connected keyboard devices and requires manual user verification.
3. Identifies and blocks anomalous keystroke patterns (e.g., too-fast intervals).

## Prerequisites
- Microsoft Windows (tested on Windows 10/11).
- Microsoft Visual Studio or any other compiler that supports Windows API.
- Administrator privileges (required for low-level hooks).

## Installation
1. Clone or download this repository.
2. Open the `.sln` file in Visual Studio (or create a new project and add the source files).
3. Build the solution in **Release** mode (recommended).

## Usage
1. **Run the compiled `.exe`**
```cmd
cd final
g++ key_logger.cpp device_authenticator.cpp behavior_analyzer.cpp main.cpp -o usb_hooks.exe -mwindows -std=c++17
```
2. The application automatically installs three hooks:
   - **Keystroke Logging Hook**: Logs every key pressed (including the originating device).
   - **Device Connection Hook**: Prompts the user to authenticate when a new keyboard is connected.
   - **Anomaly Detection Hook**: Monitors keystroke speed/intervals to detect suspicious patterns.
3. **Check the console window** (if you built a console application) or logs to see real-time events.
4. **Review `keys.log`** for all captured keystrokes (timestamp, device info, etc.).

## Contributing
- Fork this repository.
- Create a new branch for your feature.
- Submit a pull request with a clear description.

## License
This project is licensed under the [MIT License](LICENSE) - see the LICENSE file for details.

