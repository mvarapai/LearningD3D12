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
#include "RenderItem.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

void d3d_base::DrawRenderItems()
{
	mCommandList->IASetVertexBuffers(0, 1, &pStaticResources->Geometries[0].VertexBufferView);
	mCommandList->IASetIndexBuffer(&pStaticResources->Geometries[0].IndexBufferView);

	mRenderItemsDefault.at(0)->Draw(mCommandList.Get(), pDynamicResources->pCurrentFrameResource);

	mCommandList->SetPipelineState(mPSOs[2].Get());

	mRenderItemsDefault.at(1)->Draw(mCommandList.Get(), pDynamicResources->pCurrentFrameResource);

}

void d3d_base::Draw()
{
	ID3D12CommandAllocator* currCmdAlloc =
		pDynamicResources->pCurrentFrameResource->CommandListAllocator.Get();

	// Reuse the memory since the frame is processed
	ThrowIfFailed(currCmdAlloc->Reset());

	// Use the default PSO
	ThrowIfFailed(mCommandList->Reset(currCmdAlloc, mPSOs[0].Get()));


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

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
	
	// Set pass constants
	mCommandList->SetGraphicsRootConstantBufferView(0, 
		pDynamicResources->pCurrentFrameResource->PassCB->Resource()->GetGPUVirtualAddress());

	mCommandList->SetDescriptorHeaps(1, pStaticResources->mSRVHeap.GetAddressOf());

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

void d3d_base::UpdatePassCB()
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
	mPassCB.FogStart = 30.0f;
	mPassCB.FogRange = 100.0f;

	Light point = { };
	point.FalloffEnd = 20.0f;
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

void d3d_base::Update()
{
	pDynamicResources->NextFrameResource(mFence.Get());
	pDynamicResources->UpdateConstantBuffers();
	mCamera->Update();
	UpdatePassCB();
}

void d3d_base::OnMouseDown(WPARAM btnState, int x, int y)
{
	// Prepare to move
	mCamera->mLastMousePos.x = x;
	mCamera->mLastMousePos.y = y;

	// Set mouse capture on current window
	SetCapture(mhWnd);
}

void d3d_base::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void d3d_base::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		mCamera->OnMouseMove(x, y);
	}
}

// Passed from default WndProc function
LRESULT d3d_base::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
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