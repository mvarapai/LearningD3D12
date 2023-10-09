#pragma once
#include <Windows.h>
#include "timer.h"

/*	Used to initialize window and receive window messages */
struct D3DWindow {
private:
	const UINT m_x = 200;
	const UINT m_y = 200;

	HWND mhWnd = nullptr;
	HINSTANCE mhInstance = nullptr;
	LPCWSTR mClassName = L"SampleWindowClass";
	D3DWindow(HINSTANCE hInst, int show);

	Timer* mTimer = nullptr;

	// Single instance of the class
	static D3DWindow* mWindow;

public:
	HWND GetWindowHandle() const;

	// Ensure that there is only one instance of the window
	static D3DWindow* CreateD3DWindow(HINSTANCE hInst, int show);

	static D3DWindow* GetWindow();
};

