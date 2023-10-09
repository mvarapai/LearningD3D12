#include "window.h"
#include "d3dinit.h"

D3DWindow* D3DWindow::mWindow = nullptr;

// Function that processes window messages
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Constructor
D3DWindow::D3DWindow(HINSTANCE hInstance, int show) {
	mWindow = this;
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
		return;
	}

	// Registered class is ready to use. Create window and strore HWND
	mhWnd = CreateWindow(
		mClassName,
		L"Learning DirectX 12!",
		WS_OVERLAPPEDWINDOW,
		m_x,
		m_y,
		800,
		600,
		0,
		0,
		hInstance,
		0);

	// Check if window creation failed
	if (mhWnd == 0)
	{
		MessageBox(0, L"Create Window FAILED", 0, 0);
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
	
	return D3DApp::GetApp()->MsgProc(hWnd, msg, wParam, lParam);
}

// Method to secure that there is only one window instance
D3DWindow* D3DWindow::CreateD3DWindow(HINSTANCE hInst, int show)
{
	// If the window was not created before
	// Create new window
	
	return new D3DWindow(hInst, show);
}

D3DWindow* D3DWindow::GetWindow()
{
	return mWindow;
}