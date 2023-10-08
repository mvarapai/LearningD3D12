#pragma once
#include <Windows.h>

/*	Used to initialize window and receive window messages */
struct D3DWindow {
private:
	UINT m_x = 200;
	UINT m_y = 200;
	UINT mWidth = 800;
	UINT mHeight = 600;

	HWND mhWnd = nullptr;
	HINSTANCE mhInstance = nullptr;
	LPCWSTR mClassName = L"SampleWindowClass";
	D3DWindow(HINSTANCE hInst, int show, bool* result);

	// Single instance of the class
	static D3DWindow* mWindow;

public:
	HWND GetWindowHandle() const;

	// Ensure that there is only one instance of the window
	static D3DWindow* GetWindow(HINSTANCE hInst, int show, bool* result);
};

