@echo off
echo ========================================================
echo USB Attack Prevention Application - Administrator Mode
echo ========================================================
echo.
echo This mode provides full device ejection capabilities.
echo Administrator privileges are required for this functionality.
echo.
echo Running with administrator rights...
powershell -Command "Start-Process '%~dp0usb_hooks.exe' -ArgumentList '--admin-mode' -Verb RunAs"
echo.
echo If a UAC prompt appears, please click "Yes" to continue.
echo.
pause
