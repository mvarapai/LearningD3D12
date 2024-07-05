#pragma once

#include <Windows.h>

#include "d3dinit.h"

// Class that initializes and operates the window
struct D3DWindow {

private:
	
	const UINT m_x = 200;						// Initial position of
	const UINT m_y = 200;						// the window on screen

	HWND mhWnd = nullptr;						// Window handle
	HINSTANCE mhInstance = nullptr;				// Window instance
	LPCWSTR mClassName = L"SampleWindowClass";	// Class name
	
public:
	D3DWindow(HINSTANCE hInst);
	// Getter for HWND
	HWND GetWindowHandle() const;

	// Create the window and show it
	void Initialize();
	void ShowD3DWindow(int show, D3DBase* pRenderer);
};

