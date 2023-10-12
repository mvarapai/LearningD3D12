#pragma once

#include <Windows.h>

// Class that initializes and operates the window
struct D3DWindow {

private:
	
	const UINT m_x = 200;						// Initial position of
	const UINT m_y = 200;						// the window on screen

	HWND mhWnd = nullptr;						// Window handle
	HINSTANCE mhInstance = nullptr;				// Window instance
	LPCWSTR mClassName = L"SampleWindowClass";	// Class name
	
	static D3DWindow* mWindow;					// Single instance of the class

private:
	// Private constructor to prevent creation of several instances
	// Set the static class instance and HINSTANCE
	D3DWindow(HINSTANCE hInst);

public:
	// Getter for HWND
	HWND GetWindowHandle() const;

	// Getter for static class instance
	static D3DWindow* GetWindow();

	// Create the window and show it
	static D3DWindow* CreateD3DWindow(HINSTANCE hInst, int show);
};

