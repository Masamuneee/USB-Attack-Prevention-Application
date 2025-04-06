cmake_minimum_required(VERSION 3.10)

# Project name and version
project(USBMonitor VERSION 1.0.3 LANGUAGES CXX)

# Configure C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define as Windows GUI application
if(MSVC)
    # For Visual Studio
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
else()
    # For MinGW/GCC
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/final)

# Add source files
set(SOURCES
    final/main.cpp
    final/key_logger.cpp
    final/device_authenticator.cpp
    final/behavior_analyzer.cpp
    final/system_tray.cpp
)

# Add header files
set(HEADERS
    final/key_logger.h
    final/device_authenticator.h
    final/behavior_analyzer.h
    final/system_tray.h
    final/resource.h
)

# Create executable
add_executable(usb_hooks WIN32 ${SOURCES} ${HEADERS})

# Add Windows libraries
if(MSVC)
    target_link_libraries(usb_hooks setupapi comctl32)
else()
    target_link_libraries(usb_hooks setupapi comctl32)
endif()

# Installation configuration
install(TARGETS usb_hooks DESTINATION bin)
install(FILES ${CMAKE_SOURCE_DIR}/usb_icon.ico DESTINATION bin)
install(FILES ${CMAKE_SOURCE_DIR}/README.md DESTINATION docs)

# CPack configuration for creating installers
set(CPACK_PACKAGE_NAME "USBMonitor")
set(CPACK_PACKAGE_VENDOR "Masamune & Lio")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "USB Attack Prevention Application")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "USBMonitor")

set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/usb_icon.ico")
set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/usb_icon.ico")
set(CPACK_NSIS_INSTALLED_ICON_NAME "bin/usb_icon.ico")
set(CPACK_NSIS_DISPLAY_NAME "USB Attack Prevention")
set(CPACK_NSIS_PACKAGE_NAME "USB Attack Prevention")
set(CPACK_NSIS_HELP_LINK "https://github.com/Masamuneee/USB-Attack-Prevention-Application")
set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/Masamuneee/USB-Attack-Prevention-Application")

include(CPack)

# Add a simple test to verify the build
enable_testing()
add_test(NAME app_builds COMMAND usb_hooks --test-mode)
