# Changelog

All notable changes to the USB Device Security Suite will be documented in this file.

## [1.0.6] - 2025-05-15

### Added
- Improved detection of USB devices by monitoring all USB interfaces instead of just keyboards
- Enhanced authentication dialog with increased size for better visibility (500x350px)
- Larger fonts in authentication dialog for better readability (48pt for code, 24pt for status)

### Fixed
- Fixed device notification registration to properly detect all USB devices using GUID_DEVINTERFACE_USB_DEVICE
- Resolved authentication dialog issues by creating a proper window class ("AuthDlgClass")
- Improved dialog layout with increased margins and button sizes for better user experience
- Fixed proper font cleanup in authentication dialog to prevent resource leaks

### Changed
- Authentication dialog now provides more screen space and clearer visual hierarchy
- Modified device detection logic with more robust keyboard detection capabilities
- Improved error logging for authentication dialog creation

## [1.0.5] - 2025-04-30

### Added
- Enhanced device identification in trust/untrust prompts showing manufacturer, VID/PID, and location info
- Automatic device ejection when untrusting a device or after failed authentication
- New StartApp.bat for administrative device ejection operations
- Added more comprehensive device details in the UI, including last authentication time

### Fixed
- Resolved compilation errors with device ejection constants (CM_DISABLE_PERMANENTLY and CM_DISABLE_TEMPORARY)
- Fixed device ejection functionality by using the correct Configuration Manager API constants
- Improved reliability of device ejection by implementing multiple fallback approaches
- Enhanced error handling during device ejection with better user feedback

### Changed
- Improved USB Device Manager UI with better device details display and status updates
- Enhanced confirmation dialogs with comprehensive device information
- Renamed "Untrust Device" button to "Untrust & Eject" for better user understanding
- Simplified system tray menu by removing redundant options

## [1.0.4] - 2025-04-15

### Added
- Raw Input API integration for better keyboard event monitoring
- Cross-compiler compatibility with both MSVC and MinGW
- Improved device identification through Raw Input API

### Fixed
- Corrected _WIN32_WINNT redefinition warnings in MinGW builds
- Fixed localtime_s compatibility issues across different compilers
- Improved memory management in Raw Input handling
- Enhanced error handling when registering for device notifications
- Resolved GUID definition issues with multiple compilation units
- Fixed build pipeline to properly handle GUID initialization

### Changed
- Optimized header file structure with proper include organization
- Improved code documentation and commenting style
- Enhanced build pipeline with custom version specification
- Updated CI/CD workflow to allow optional releases with custom versioning

## [1.0.3] - 2025-04-06

### Added
- Device untrusting functionality to revoke trust from previously authorized devices
- Settings dialog with application configuration options
- Blacklist word management through the UI
- Custom system tray icon for better visibility
- "About" dialog with application information
- Option to launch application at Windows startup

### Fixed
- Improved authentication dialog with better visual feedback
- Enhanced ListView control for device management
- Fixed memory leaks in device event handling

### Changed
- Modernized UI with improved styling and layout
- Added confirmation dialogs for critical operations
- Enhanced device details display
- Better error handling and user feedback

## [1.0.2] - 2025-03-04

### Added
- System tray integration for easier application management
- Device list view to monitor and manage connected devices
- Option to toggle keystroke logging

### Fixed
- Fixed key logger not writing to keys.log file
- Resolved compatibility issues with MinGW compiler
- Addressed linking problems with setupapi library
- Fixed ListView control implementation for device display

### Changed
- Improved device authentication dialog
- Enhanced error handling throughout the application
- Updated USB device detection for better compatibility

## [1.0.1] - 2025-02-23

### Added
- Behavior analysis to detect suspicious keystroke patterns
- Blacklist functionality for potentially malicious commands
- Automatic blocking of devices after authentication failures

### Fixed
- Memory leaks in device management code
- UI responsiveness issues during device authentication

## [1.0.0] - 2025-02-03

### Added
- Initial release with basic USB keyboard authentication
- Key logging functionality with timing analysis
- Device connection and disconnection monitoring
