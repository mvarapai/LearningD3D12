#include "d3dinit.h"

#include <Windows.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <windowsx.h>

#include "d3dUtil.h"
#include "window.h"
#include "timer.h"
#include "UploadBuffer.h"
#include "d3dinit.h"
#include "drawable.h"
#include "d3dapp.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

void D3DApplication::DrawRenderItems()
{
	mCommandList->SetGraphicsRootSignature(mDefaultShader.mRootSignature.Get());

	// Set pass constants
	mCommandList->SetGraphicsRootConstantBufferView(0,
		pDynamicResources->pCurrentFrameResource->PassCB->Resource()->GetGPUVirtualAddress());

	mCommandList->SetDescriptorHeaps(1, pStaticResources->mSRVHeap.GetAddressOf());

	GEOMETRY_DESCRIPTOR& defaultGeometry = pStaticResources->Geometries[0];
	DefaultDrawable::SetVBAndIB(mCommandList.Get(), defaultGeometry.VertexBufferView, defaultGeometry.IndexBufferView);

	mTerrain->Draw(mCommandList.Get(), pDynamicResources->pCurrentFrameResource);

	mCommandList->SetPipelineState(mBlendPSO.Get());

	mWater->Draw(mCommandList.Get(), pDynamicResources->pCurrentFrameResource);

}

void D3DApplication::Draw()
{
	ID3D12CommandAllocator* currCmdAlloc =
		pDynamicResources->pCurrentFrameResource->CommandListAllocator.Get();

	// Reuse the memory since the frame is processed
	ThrowIfFailed(currCmdAlloc->Reset());

	// Use the default PSO
	ThrowIfFailed(mCommandList->Reset(currCmdAlloc, mDefaultPSO.Get()));


	// To know what to render
	mCommandList->RSSetViewports(1, &mViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	Transition(GetCurrentBackBuffer(),
		mCommandList.Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);

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

	// Draw objects
	DrawRenderItems();

	// When rendered, change state to present
	Transition(GetCurrentBackBuffer(),
		mCommandList.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);

	// Done recording commands
	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));

	// Swap buffers
	mCurrBackBuffer = (mCurrBackBuffer + 1) % swapChainBufferCount;

	// Set fence point for current frame resource
	pDynamicResources->pCurrentFrameResource->Fence = ++mCurrentFence;
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void D3DApplication::UpdatePassCB()
{
	PassConstants mPassCB;

	XMMATRIX view = XMLoadFloat4x4(&mCamera->mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);

	XMVECTOR viewDet = XMMatrixDeterminant(view);
	XMMATRIX invView = XMMatrixInverse(&viewDet, view);

	XMVECTOR projDet = XMMatrixDeterminant(proj);
	XMMATRIX invProj = XMMatrixInverse(&projDet, proj);

	XMVECTOR viewProjDet = XMMatrixDeterminant(viewProj);
	XMMATRIX invViewProj = XMMatrixInverse(&viewProjDet, viewProj);


	XMStoreFloat4x4(&mPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mPassCB.InvViewProj, XMMatrixTranspose(invViewProj));

	XMStoreFloat3(&mPassCB.EyePosW, XMLoadFloat4(&mCamera->mPosition));

	mPassCB.NearZ = 1.0f;
	mPassCB.FarZ = 1000.0f;
	mPassCB.TotalTime = mTimer->TotalTime();
	mPassCB.DeltaTime = mTimer->DeltaTime();

	mPassCB.AmbientLight = { 0.25f, 0.25f, 0.25f, 1.0f };

	XMStoreFloat4(&mPassCB.FogColor, DirectX::Colors::Gray.v);
	mPassCB.FogStart = 100.0f;
	mPassCB.FogRange = 200.0f;

	Light point = { };
	point.FalloffEnd = 100.0f;
	point.FalloffStart = 0.1f;
	point.Position = { 0.0f, 12.0f, 0.0f };
	point.Strength = { 1.0f, 1.0f, 1.0f };

	Light dir = { };
	dir.Direction = { 0.0f, -0.6f, -0.8f };
	dir.Strength = { 1.0f, 1.0f, 1.0f };

	//mPassCB.Lights[1] = point;
	//mPassCB.Lights[0] = dir;
	mPassCB.Lights[0] = dir;

	UploadBuffer<PassConstants>* currPassCB = pDynamicResources->pCurrentFrameResource->PassCB.get();
	currPassCB->CopyData(0, mPassCB);
}

void D3DApplication::Update()
{
	pDynamicResources->NextFrameResource(mFence.Get());
	pDynamicResources->UpdateConstantBuffers();
	mCamera->Update();
	UpdatePassCB();
}

void D3DApplication::OnMouseDown(WPARAM btnState, int x, int y)
{
	// Prepare to move
	mCamera->mLastMousePos.x = x;
	mCamera->mLastMousePos.y = y;

	// Set mouse capture on current window
	SetCapture(mhWnd);
}

void D3DApplication::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void D3DApplication::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		mCamera->OnMouseMove(x, y);
	}
}

// Wait till GPU finishes with all commands
void D3DBase::FlushCommandQueue()
{
	// Advance the fence value
	mCurrentFence++;

	// Set a new fence point
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false,
			EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));

		if (eventHandle == nullptr) return;

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

// Called to change back and DS buffer sizes.
// - Swap chain buffers are resized and new RTV is created.
// - DS buffer is recreated with new dimensions along with DSV
// - Command list is executed
// - Viewport and scissor rects are reset
// - Due to change of aspect ratio projection matrix is changed
void D3DBase::OnResize()
{
	// Make sure all GPU commands are executed to avoid resource hazard
	FlushCommandQueue();

	// Reset the command list
	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(),
		nullptr));

	// Release previous resources
	for (int i = 0; i < swapChainBufferCount; i++)
	{
		mSwapChainBuffer[i].Reset();
	}
	mDepthStencilBuffer.Reset();

	// Resize the swap chain

	ThrowIfFailed(mSwapChain->ResizeBuffers(
		swapChainBufferCount,
		mClientWidth,
		mClientHeight,
		mBackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	mCurrBackBuffer = 0;

	D3DHelper::CreateRTVs(mRtvHeap.Get(), swapChainBufferCount,
		mSwapChain.Get(), md3dDevice.Get(), mSwapChainBuffer);
	D3DHelper::CreateDepthStencilBufferAndView(mClientWidth, mClientHeight,
		mDepthStencilFormat, mDsvHeap.Get(), mCommandList.Get(),
		md3dDevice.Get(), mDepthStencilBuffer.GetAddressOf());

	// The only command is resource barrier
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	FlushCommandQueue();

	// Set the viewport
	mViewport = { };
	mViewport.TopLeftX = 0.0f;
	mViewport.TopLeftY = 0.0f;
	mViewport.Width = static_cast<float>(mClientWidth);
	mViewport.Height = static_cast<float>(mClientHeight);
	mViewport.MinDepth = 0.0f;
	mViewport.MaxDepth = 1.0f;

	// Set the scissor rects
	mScissorRect = { 0, 0, (long)mClientWidth,
		(long)mClientHeight };
}



// Passed from default WndProc function
LRESULT D3DBase::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		// When the window is either activated or deactivated
	case WM_ACTIVATE:
		// If deactivated, pause
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			mAppPaused = true;
			mTimer->Stop();
		}
		// Else, start the timer
		else
		{
			mAppPaused = false;
			mTimer->Start();
		}
		return 0;

		// Called when user resizes the window
	case WM_SIZE:
		mClientWidth = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);

		if (md3dDevice)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				mAppPaused = true;
				mMinimized = true;
				mMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				mAppPaused = false;
				mMinimized = false;
				mMaximized = true;
				// Since it is a change in window size,
				// and the window is visible, redraw
				OnResize();
			}
			else if (wParam == SIZE_RESTORED)
			{
				if (mMinimized)
				{
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}
				else if (mMaximized)
				{
					mAppPaused = false;
					mMaximized = false;
					OnResize();
				}
				else if (mResizing)
				{
					// While the window is resizing, we wait.
					// It is done in order to save on performance,
					// because recreating swap chain buffers every
					// frame would be too resource ineffective.
				}
				else
				{
					// Any other call, we resize the buffers
					OnResize();
				}
			}
		}
		return 0;

		// Pause the app when window is resizing
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing = true;
		mTimer->Stop();
		return 0;
		// Resume when resize button is released
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing = false;
		mTimer->Start();
		OnResize();
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

		// Process unhandled menu calls
	case WM_MENUCHAR:
		return MAKELRESULT(0, MNC_CLOSE);

		// Prevent the window from becoming too small
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_KEYDOWN:
		return 0;
	case WM_KEYUP:
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
			return 0;
		}
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}