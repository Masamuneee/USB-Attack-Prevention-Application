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

REM First compile the resource file
echo Compiling resources...
windres final\resource.rc -o resource.o

echo Compiling the application...
REM Only need to define INITGUID once, remove it from device_authenticator.cpp
g++ -c -DINITGUID final\device_authenticator.cpp -o device_authenticator.o
g++ -c final\key_logger.cpp final\behavior_analyzer.cpp final\system_tray.cpp final\main.cpp

REM Link everything together
g++ device_authenticator.o key_logger.o behavior_analyzer.o system_tray.o main.o resource.o -o usb_hooks.exe -mwindows -std=c++17 -lsetupapi -lcomctl32 -Wl,-subsystem,windows

REM Return status based on compilation success
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Compilation failed with error code %ERRORLEVEL%.
    echo Please check the error messages above.
    echo.
    exit /b %ERRORLEVEL%
)

REM Clean up object files
del *.o

echo.
echo Compilation successful!
echo The executable has been created: usb_hooks.exe
echo.
pause