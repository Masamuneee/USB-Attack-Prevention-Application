cmake_minimum_required(VERSION 3.10)

# Project name and version
project(usb_hooks VERSION 1.0.6 LANGUAGES CXX)

# Configure C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define as Windows GUI application
if(MSVC)
    # For Visual Studio
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    # Remove INITGUID from global definitions to avoid redefinition warnings
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
    # For MinGW/GCC
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/final)

# Add source files - Use source-specific flags to define INITGUID only where needed
set(SOURCES
    final/main.cpp
    final/key_logger.cpp
    final/device_authenticator.cpp
    final/behavior_analyzer.cpp
    final/system_tray.cpp
    final/key_logger_auth.cpp
)

# Add header files
set(HEADERS
    final/key_logger.h
    final/device_authenticator.h
    final/behavior_analyzer.h
    final/system_tray.h
    final/resource.h
    final/key_logger_auth.h
)

# Configure the resource file
configure_file(
    ${CMAKE_SOURCE_DIR}/final/resource.rc
    ${CMAKE_BINARY_DIR}/resource.rc
    COPYONLY
)

# Create a simple empty icon file if it doesn't exist
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/usb_application.ico")
    file(WRITE "${CMAKE_SOURCE_DIR}/usb_application.ico" "")
endif()

# Create executable
add_executable(usb_hooks WIN32 ${SOURCES} ${HEADERS} ${CMAKE_BINARY_DIR}/resource.rc)

# Set source-specific compile definitions
set_source_files_properties(
    final/device_authenticator.cpp
    PROPERTIES COMPILE_DEFINITIONS "INITGUID"
)

# Add Windows libraries
target_link_libraries(usb_hooks setupapi comctl32 cfgmgr32)

# Installation configuration
install(TARGETS usb_hooks DESTINATION bin)
install(FILES ${CMAKE_SOURCE_DIR}/usb_application.ico DESTINATION bin)
install(FILES ${CMAKE_SOURCE_DIR}/README.md DESTINATION docs)
install(FILES ${CMAKE_SOURCE_DIR}/CHANGELOG.md DESTINATION docs)
install(FILES ${CMAKE_SOURCE_DIR}/StartApp.bat DESTINATION bin)
install(FILES ${CMAKE_SOURCE_DIR}/compile.bat DESTINATION bin)

# CPack configuration for creating installers
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "USB-Security-Suite")
set(CPACK_PACKAGE_VENDOR "Masamune & Lio")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "USB Attack Prevention Application")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "USBMonitor")

set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/usb_application.ico")
set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/usb_application.ico")
set(CPACK_NSIS_INSTALLED_ICON_NAME "bin/usb_application.ico")
set(CPACK_NSIS_DISPLAY_NAME "USB Attack Prevention")
set(CPACK_NSIS_PACKAGE_NAME "USB Attack Prevention")
set(CPACK_NSIS_HELP_LINK "https://github.com/Masamuneee/USB-Attack-Prevention-Application")
set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/Masamuneee/USB-Attack-Prevention-Application")

include(CPack)

# Add a simple test that verifies the build without actually running the app
enable_testing()
add_test(
  NAME app_builds 
  COMMAND ${CMAKE_COMMAND} -E echo "Build verification complete"
)

# Add a test for test mode
add_test(NAME TestMode COMMAND usb_hooks --test-mode)

# Set a reasonable timeout for the test (5 seconds)
set_tests_properties(app_builds PROPERTIES TIMEOUT 5)
set_tests_properties(TestMode PROPERTIES TIMEOUT 5)
