/*
* main.cpp
* 
* Initializes the window and DirectX
* 
* Mikalai Varapai, 25.09.2023
*/

#include <Windows.h>
#include <DirectXColors.h>

#include "window.h"
#include "timer.h"
#include "d3dinit.h"
#include "d3dUtil.h"

Timer* g_timer = nullptr;

// Entry point to the app
int WINAPI WinMain(_In_ HINSTANCE hInstance,// Handle to app in Windows
	_In_opt_ HINSTANCE hPrevInstance,		// Not used
	_In_ PSTR pCmdLine,						// Command line (PSTR = char*)
	_In_ int nCmdShow)						// Show Command
{
	// Create timer
	g_timer = new Timer();
	g_timer->Reset();
	g_timer->Start();

	D3DApp::Initialize(hInstance, nCmdShow, g_timer);

	D3DApp::GetApp()->Run();
	return 0;
}

// Main program cycle
int D3DApp::Run()
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

void D3DApp::Draw()
{
	// Reuse the memory
	ThrowIfFailed(mCommandAllocator->Reset());
	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

	ResourceBarrier* barrier = new ResourceBarrier(mCommandList.Get());
	barrier->Transition(GetCurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);

	// To know what to render
	mCommandList->RSSetViewports(1, &mViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Clear the back and depth buffer
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(),
		DirectX::Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f, 0, 0, nullptr);

	const D3D12_CPU_DESCRIPTOR_HANDLE currBackBufferHandle = CurrentBackBufferView();
	const D3D12_CPU_DESCRIPTOR_HANDLE currDepthBufferHandle = DepthStencilView();

	// Set render target
	mCommandList->OMSetRenderTargets(1, &currBackBufferHandle,
		true, &currDepthBufferHandle);

	// When rendered, change state to present
	barrier->Transition(GetCurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);

	// Done recording commands
	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));

	// Swap buffers
	mCurrBackBuffer = (mCurrBackBuffer + 1) % swapChainBufferCount;

	// Wait per frame (to be redone soon)
	FlushCommandQueue();
}

void D3DApp::Update()
{

}