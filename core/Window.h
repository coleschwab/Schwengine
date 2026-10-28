#pragma once
#define WIN32_Lean_AND_MEAN
#include <windows.h>
#include <string>

class Window {
public:
	Window(int width, int height, std::string title);
	~Window();

	bool processMessages();
	HWND getHandle() const { return hwnd; }
	
private:
	HWND hwnd;
	HINSTANCE hInstance;
};