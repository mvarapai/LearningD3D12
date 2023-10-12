#include "window.h"
#include "d3dinit.h"

// Initialize static instance with nullptr
D3DWindow* D3DWindow::mWindow = nullptr;

// Forward declaration to be used for creation
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Constructor to set static instance and window instance
D3DWindow::D3DWindow(HINSTANCE hInstance) {
	// Set static instance
	mWindow = this;
	// Set hInstance
	mhInstance = hInstance;
}

// Method to secure that there is only one window instance
D3DWindow* D3DWindow::CreateD3DWindow(HINSTANCE hInstance, int show)
{
	// If there is no window
	if (mWindow == nullptr)
	{
		// Set instance and static instance
		D3DWindow* window = new D3DWindow(hInstance);

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
		wc.lpszClassName = window->mClassName;

		// Try to create class
		if (!RegisterClass(&wc))
		{
			MessageBox(0, L"RegisterClass FAILED", 0, 0);
			return nullptr;
		}

		// Registered class is ready to use. Create window and strore HWND
		window->mhWnd = CreateWindow(
			window->mClassName,
			L"Learning DirectX 12!",
			WS_OVERLAPPEDWINDOW,
			window->m_x,
			window->m_y,
			800,
			600,
			0,
			0,
			hInstance,
			0);

		// Check if window creation failed
		if (window->mhWnd == 0)
		{
			MessageBox(0, L"Create Window FAILED", 0, 0);
			return nullptr;
		}

		// Finally, when window is created show it
		ShowWindow(window->mhWnd, show);
		UpdateWindow(window->mhWnd);


		return window;
	}
	
	// In case window already exists

	return mWindow;
}

// Returns static instance of the window
D3DWindow* D3DWindow::GetWindow() { return mWindow; }

// Returns HWND for an object
HWND D3DWindow::GetWindowHandle() const { return mhWnd; }

// Process window messages, passes arguments to D3DApp static class
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return D3DApp::GetApp()->MsgProc(hWnd, msg, wParam, lParam);
}