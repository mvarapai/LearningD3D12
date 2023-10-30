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

void D3DApp::DrawRenderItems()
{
	UINT objCBByteSize = CalcConstantBufferByteSize(sizeof(ObjectConstants));

	ID3D12Resource* objectCB = mCurrFrameResource->ObjectCB->Resource();

	// Iterate through render items
	for (UINT i = 0; i < mAllRenderItems.size(); i++)
	{
		RenderItem* ri = mAllRenderItems[i].get();

		D3D12_VERTEX_BUFFER_VIEW vbv = ri->Geo->VertexBufferView();
		D3D12_INDEX_BUFFER_VIEW ibv = ri->Geo->IndexBufferView();

		mCommandList->IASetVertexBuffers(0, 1, &vbv);
		mCommandList->IASetIndexBuffer(&ibv);
		mCommandList->IASetPrimitiveTopology(ri->PrimitiveType);

		mCommandList->SetGraphicsRootDescriptorTable(1,
			GetPerObjectCBV(mCurrFrameResourceIndex, i));

		SubmeshGeometry submesh = ri->Geo->DrawArgs[ri->Submesh];
		mCommandList->DrawIndexedInstanced(submesh.IndexCount, 1, submesh.StartIndexLocation,
			submesh.BaseVertexLocation, 0);
	}
}

void D3DApp::Draw()
{
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> currCmdAlloc =
		mCurrFrameResource->CommandListAllocator;

	// Reuse the memory since the frame is processed
	ThrowIfFailed(currCmdAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(currCmdAlloc.Get(), mPSO.Get()));


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

	// Code to draw the cube

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps),
		descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
	
	// Set pass constants
	mCommandList->SetGraphicsRootDescriptorTable(0,
		GetPassCBV(mCurrFrameResourceIndex));

	// Deaw objects
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
	mCurrFrameResource->Fence = ++mCurrentFence;
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void D3DApp::UpdateCamera()
{
	// Convert spherical to Cartesian coordinates
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	// Build the view matrix
	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

// Stores world matrices to current frame's per object CB
void D3DApp::UpdateObjectCBs()
{
	UploadBuffer<ObjectConstants>* currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (std::unique_ptr<RenderItem>& renderItem : mAllRenderItems)
	{
		// Only update 'dirty' frames - the ones with old data
		if (renderItem->numFramesDirty > 0)
		{
			// Load world matrix from the structure
			XMMATRIX world = XMLoadFloat4x4(&renderItem->World);

			// Write it to the CB structure
			ObjectConstants objConstants = { };
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));

			// Copy data to current frame's per object CB
			currObjectCB->CopyData(renderItem->ObjCBIndex, objConstants);

			// One frame updated
			renderItem->numFramesDirty--;
		}
	}
}

void D3DApp::UpdatePassCB()
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);

	XMStoreFloat4x4(&mPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mPassCB.ViewProj, XMMatrixTranspose(viewProj));

	UploadBuffer<PassConstants>* currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mPassCB);
}

void D3DApp::Update()
{
	// Get view matrix ready for copy to current frame's pass CB
	UpdateCamera();

	// Write to next frame resource
	mCurrFrameResourceIndex =
		(mCurrFrameResourceIndex + 1) % NumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Check whether the GPU has finished processing current frame
	if (mCurrFrameResource->Fence != 0
		&& mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	// Then, after the frame is processed we can update object and pass CBs,
	// which write to frame resources
	UpdateObjectCBs();
	UpdatePassCB();
}

void D3DApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	// Prepare to move
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	// Set mouse capture on current window
	SetCapture(D3DWindow::GetWindow()->GetWindowHandle());
}

void D3DApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void D3DApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree
		float dx = XMConvertToRadians
		(0.25f * static_cast<float>(x - mLastMousePos.x));

		float dy = XMConvertToRadians
		(0.25f * static_cast<float>(y - mLastMousePos.y));

		mTheta += dx;
		mPhi += dy;

		mRadius += dx - dy;

		mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
	}

	// Update last position
	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

// Passed from default WndProc function
LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
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

	case WM_KEYUP:
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
			return 0;
		}
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}