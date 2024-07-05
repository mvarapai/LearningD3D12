#include "window.h"
#include "d3dinit.h"

D3DBase* pD3D = nullptr;

// Forward declaration to be used for creation
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

D3DWindow::D3DWindow(HINSTANCE hInst) : mhInstance(hInst)
{
}

// Method to secure that there is only one window instance
void D3DWindow::Initialize()
{
	
	// Fill out structure with window class data
	WNDCLASS wc = {};
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = mhInstance;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = mClassName;

	// Try to create class
	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass FAILED", 0, 0);
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
		mhInstance,
		0);

	// Check if window creation failed
	if (mhWnd == 0)
	{
		MessageBox(0, L"Create Window FAILED", 0, 0);
	}
}

// Returns HWND for an object
HWND D3DWindow::GetWindowHandle() const { return mhWnd; }

void D3DWindow::ShowD3DWindow(int show, D3DBase* pRenderer)
{
	pD3D = pRenderer;
	// Finally, when window is created show it
	ShowWindow(mhWnd, show);
	UpdateWindow(mhWnd);
}

// Process window messages, passes arguments to d3d_base static class
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return pD3D->MsgProc(hWnd, msg, wParam, lParam);
}