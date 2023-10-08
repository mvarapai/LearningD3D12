#include "window.h"

D3DWindow* D3DWindow::mWindow = nullptr;

// Forward declarations

// Function that processes window messages
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Constructor
D3DWindow::D3DWindow(HINSTANCE hInstance, int show, bool* result) {
	*result = true;
	this->mhInstance = hInstance;

	// Fill out structure with window class data
	WNDCLASS wc = {};
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = mClassName;

	// Try to create class
	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass FAILED", 0, 0);
		*result = false;
		return;
	}

	// Registered class is ready to use. Create window and strore HWND
	mhWnd = CreateWindow(
		mClassName,
		L"Learning DirectX 12!",
		WS_OVERLAPPEDWINDOW,
		m_x,
		m_y,
		mWidth,
		mHeight,
		0,
		0,
		hInstance,
		0);

	// Check if window creation failed
	if (mhWnd == 0)
	{
		MessageBox(0, L"Create Window FAILED", 0, 0);
		*result = false;
		return;
	}

	// Finally, when window is created show it
	ShowWindow(mhWnd, show);
	UpdateWindow(mhWnd);
}

// Getter of a window handle
HWND D3DWindow::GetWindowHandle() const
{
	return mhWnd;
}

// Process window messages
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Handle some messages that are not processed by default WndProc:
	switch (msg)
	{
		// If ESC key is pressed:
	case WM_KEYDOWN:
		// (Potential for some "ProcessKeyboardInput" function)
		if (wParam == VK_ESCAPE)
			DestroyWindow(hWnd);
		return 0;

		// If we get a destroy message terminate the loop in main.cpp
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	// Everything else is processed as by default
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

// Method to secure that there is only one window instance
D3DWindow* D3DWindow::GetWindow(HINSTANCE hInst, int show, bool* result)
{
	// If the window was not created before
	if (mWindow == nullptr) {
		// Create new window
		mWindow = new D3DWindow(hInst, show, result);
	}
	return mWindow;
}
