#pragma once
#include <Windows.h>

/*	Used to initialize window and receive window messages */
struct ID3DWindow {
private:
	UINT m_x = 200;
	UINT m_y = 200;
	UINT mWidth = 600;
	UINT mHeight = 400;

	HWND mhWnd = nullptr;
	HINSTANCE mhInstance = nullptr;
	LPCWSTR mClassName = L"SampleWindowClass";

public:
	ID3DWindow(HINSTANCE hInst, int show, bool* result);
	HWND GetWindowHandle() const;
};