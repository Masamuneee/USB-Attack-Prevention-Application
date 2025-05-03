#ifndef KEY_LOGGER_AUTH_H
#define KEY_LOGGER_AUTH_H

#include <windows.h>
#include <string>

// Functions to handle keyboard input during authentication
void StartKeyboardCapture(const std::string& deviceId, HWND dialogWindow);
void StopKeyboardCapture();

#endif // KEY_LOGGER_AUTH_H
