@echo off
REM Compile helper for USB monitor application

echo Checking for running instances...
taskkill /f /im usb_hooks.exe 2>nul
if %ERRORLEVEL% EQU 0 (
    echo Application terminated successfully.
    echo Waiting for resources to be released...
    timeout /t 2 /nobreak >nul
) else (
    echo No running instance found, proceeding with compilation.
)

echo Compiling the application...
g++ final\key_logger.cpp final\device_authenticator.cpp final\behavior_analyzer.cpp final\system_tray.cpp final\main.cpp -o usb_hooks.exe -mwindows -std=c++17 -lsetupapi -lcomctl32

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Compilation successful!
    echo The executable has been created: usb_hooks.exe
) else (
    echo.
    echo Compilation failed with error code %ERRORLEVEL%.
    echo Please check the error messages above.
)

echo.
pause