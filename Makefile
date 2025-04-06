all: usb_hooks.exe

usb_hooks.exe: final/key_logger.cpp final/device_authenticator.cpp final/behavior_analyzer.cpp final/system_tray.cpp final/main.cpp
	g++ $^ -o $@ -mwindows -std=c++17 -lsetupapi -lcomctl32

clean:
	del usb_hooks.exe
