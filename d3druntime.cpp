#include "d3dinit.h"

#include <Windows.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <DirectXColors.h>

#include "d3dUtil.h"
#include "window.h"
#include "timer.h"
#include "UploadBuffer.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

void D3DApp::Draw()
{
	// Reuse the memory
	ThrowIfFailed(mCommandAllocator->Reset());
	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), mPSO.Get()));


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

	D3D12_VERTEX_BUFFER_VIEW VBView = mBoxGeo->VertexBufferView();
	D3D12_INDEX_BUFFER_VIEW IBView = mBoxGeo->IndexBufferView();

	mCommandList->IASetVertexBuffers(0, 1, &VBView);
	mCommandList->IASetIndexBuffer(&IBView);
	
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Bind CBV to root table
	mCommandList->SetGraphicsRootDescriptorTable(0,
		mCbvHeap->GetGPUDescriptorHandleForHeapStart());

	mCommandList->DrawIndexedInstanced(mBoxGeo->DrawArgs["box"].IndexCount, 1, 0, 0, 0);
	
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

	// Wait per frame (to be redone soon)
	FlushCommandQueue();
}

void D3DApp::Update()
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

	// World matrix left unchanged
	XMMATRIX world = XMLoadFloat4x4(&mWorld);

	// Updated per OnResize() call
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX worldViewProj = world * view * proj;

	ObjectConstants objConstants = { };
	XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
	mObjectCB->CopyData(0, objConstants);
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