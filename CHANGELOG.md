# Changelog

All notable changes to the USB Device Security Suite will be documented in this file.

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
