/*
* main.cpp
* 
* Initializes the window and DirectX
* 
* Mikalai Varapai, 25.09.2023
*/

#include <Windows.h>
#include "window.h"
#include "timer.h"

// ===============================
//		Global variables
// ===============================

ID3DWindow* g_wnd = nullptr;
Timer* g_timer = nullptr;

bool g_appPaused = false;

// ===============================
//		Function templates
// ===============================

// Entry point to the app
int WINAPI WinMain(_In_ HINSTANCE hInstance,// Handle to app in Windows
	_In_opt_ HINSTANCE hPrevInstance,		// Not used
	_In_ PSTR pCmdLine,						// Command line (PSTR = char*)
	_In_ int nCmdShow);						// Show Command

// Program cycle
int run();

// ===============================
//		Function implementations
// ===============================

int WINAPI WinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ PSTR pCmdLine,
	_In_ int nCmdShow)
{

	// Create window
	bool result = true;
	g_wnd = new ID3DWindow(hInstance, nCmdShow, &result);

	// Create timer
	g_timer = new Timer();
	g_timer->Reset();
	g_timer->Start();

	if (result)
		run();

	return 0;
}

// Main program cycle
int run()
{
	// Process messages
	MSG msg = { 0 };

	// Loop until we get a WM_QUIT message
	// GetMessage puts the thread at sleep until gets a message
	// Peek message returns nothing if there are no messages (e.g. not sleeps)

	while (msg.message != WM_QUIT)
	{
		// If there are Windows messages, process them
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// Otherwise, process application and DirectX messages
		else {
			g_timer->Tick();
			// TODO: display FPS at title
			if (!g_appPaused)
			{
				
			}
			else
			{
				Sleep(100);
			}

		}
	}
	return (int)msg.wParam;
}