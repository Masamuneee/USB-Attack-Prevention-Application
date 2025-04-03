all: usb_hooks.exe

usb_hooks.exe: key_logger.cpp device_authenticator.cpp behavior_analyzer.cpp system_tray.cpp main.cpp
	g++ $^ -o $@ -mwindows -std=c++17 -lsetupapi

clean:
	del usb_hooks.exe
