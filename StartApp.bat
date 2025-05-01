@echo off
echo ========================================================
echo USB Attack Prevention - Device Ejection Tool
echo ========================================================
echo.
echo This tool will run the USB Attack Prevention application
echo with elevated privileges specifically for ejecting devices.
echo.
echo Administrator privileges are required for proper device ejection.
echo.
echo Running with administrator rights...
powershell -Command "Start-Process '%~dp0usb_hooks.exe' -ArgumentList '--admin-mode --eject-mode' -Verb RunAs"
echo.
echo If a UAC prompt appears, please click "Yes" to continue.
echo.
pause
