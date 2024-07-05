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
#include "d3dinit.h"
#include "d3dUtil.h"
#include "d3dapp.h"

// Entry point to the app
int WINAPI WinMain(_In_ HINSTANCE hInstance,// Handle to app in Windows
	_In_opt_ HINSTANCE hPrevInstance,		// Not used
	_In_ PSTR pCmdLine,						// Command line (PSTR = char*)
	_In_ int nCmdShow)						// Show Command
{
	D3DWindow window = { hInstance };
	window.Initialize();

	D3DApplication app;
	app.Initialize(window.GetWindowHandle());

	window.ShowD3DWindow(nCmdShow, &app);
	app.Run();
	return 0;
}

// Main program cycle
int D3DBase::Run()
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
			mTimer->Tick();
			// TODO: display FPS at title
			if (!mAppPaused)
			{
				CalculateFrameStats();
				Update();
				Draw();
			}
			else
			{
				Sleep(100);
			}

		}
	}
	return (int)msg.wParam;
}