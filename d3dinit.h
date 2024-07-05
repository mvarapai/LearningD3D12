/*****************************************************************//**
 * \file   d3dinit.h
 * \brief  Main header file, contains all variables for operating D3D.
 * 
 * \author Mikalai Varapai
 * \date   September 2023
 *********************************************************************/

#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include <string>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <memory>

#include "timer.h"
#include "d3dUtil.h"
#include "UploadBuffer.h"
#include "FrameResource.h"
#include "drawable.h"
#include "structures.h"
#include "geometry.h"
#include "d3dcamera.h" 
#include "d3dcomponent.h"

#include "d3dresource.h"

/**
 * Class that initializes basic DirectX 12 interfaces and
 * operates the window.
 */
class D3DBase
{
private:
	virtual void InitializeComponents() = 0;

public:
	// Initialize the window and DirectX
	void Initialize(HWND hWnd)
	{
		mhWnd = hWnd;

		mTimer = std::make_unique<Timer>();
		mTimer->Reset();
		mTimer->Start();

#if defined(DEBUG) || defined(_DEBUG)
		D3DHelper::EnableDebugInterface(mDebugController.GetAddressOf());
#endif
		D3DHelper::CreateDevice(mdxgiFactory.GetAddressOf(), md3dDevice.GetAddressOf());
		D3DHelper::CreateFence(md3dDevice.Get(), mFence.GetAddressOf());
		D3DHelper::CreateCommandObjects(md3dDevice.Get(),
			mCommandList.GetAddressOf(),
			mCommandAllocator.GetAddressOf(),
			mCommandQueue.GetAddressOf());
		D3DHelper::CreateSwapChain(mClientWidth, mClientHeight,
			mBackBufferFormat, 2, mhWnd, true, mdxgiFactory.Get(),
			mCommandQueue.Get(), mSwapChain.GetAddressOf());
		D3DHelper::CreateRTVAndDSVDescriptorHeaps(2, md3dDevice.Get(),
			mRtvHeap.GetAddressOf(), mDsvHeap.GetAddressOf());

		// Query descriptor increment sizes
		mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		OnResize();		// Executes command list

#if defined(DEBUG) || defined(_DEBUG)
		LogAdapters();	// If in debug mode, log adapters
#endif
		ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

		// Leave it to the rendering program to create any component objects
		InitializeComponents();

		// Execute initialization commands
		ThrowIfFailed(mCommandList->Close());
		ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
		mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

		// Wait till all commands are executed
		FlushCommandQueue();
	}

	D3DBase() = default;
private:
	// Forbid copying
	D3DBase(D3DBase& rhs) = delete;
	D3DBase& operator=(D3DBase& rhs) = delete;

protected:
	HWND												mhWnd;
	std::unique_ptr<Timer>								mTimer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Debug>					mDebugController = nullptr;
	Microsoft::WRL::ComPtr<IDXGIFactory4>				mdxgiFactory = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Device>				md3dDevice = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Fence>					mFence = nullptr;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue>			mCommandQueue = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		mCommandAllocator = nullptr;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	mCommandList = nullptr;

	const DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	const DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	D3D12_VIEWPORT mViewport = { };
	D3D12_RECT mScissorRect = { };

	int	mCurrBackBuffer = 0;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		mRtvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		mDsvHeap = nullptr;

	UINT mRtvDescriptorSize = 0;
	UINT mCbvSrvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;

	static const int									swapChainBufferCount = 2;
	Microsoft::WRL::ComPtr<IDXGISwapChain>				mSwapChain = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource>				mDepthStencilBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource>				mSwapChainBuffer[swapChainBufferCount];

	/******************************************************
	 *					Variables
	 ******************************************************/

protected:

	UINT64 mCurrentFence = 0;

protected:
	int msaaQualityLevels = 0;
	bool msaaEnabled = false;
private:
	// Window state members
	std::wstring mMainWindowCaption = L"Learning DirectX12";

	// Window management variables
	bool mAppPaused = false;
	bool mMaximized = false;
	bool mMinimized = false;
	bool mResizing = false;		// Used to terminate drawing while resizing

private:

	// Initial window dimensions, changed per onResize() call
	UINT mClientWidth = 800;
	UINT mClientHeight = 600;

/**********************************************************
 *				Initialization functions
 **********************************************************/
public:
	
	LRESULT MsgProc(HWND hwnd, UINT msg,		// Function for message processing,
		WPARAM wParam, LPARAM lParam);			// called from WndProc of D3DWindow


	/**********************************************************
	*				Getter functions
	**********************************************************/
protected:
	
	int GetMSAAQualityLevels();					// Get max quality levels for 4X MSAA
	float AspectRatio()
	{
		return static_cast<float>(mClientWidth) / mClientHeight;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += (SIZE_T)mRtvDescriptorSize * mCurrBackBuffer;
		return handle;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const
	{
		return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
	}
	ID3D12Resource* GetCurrentBackBuffer()
	{
		return mSwapChainBuffer[mCurrBackBuffer].Get();
	}
public:
	bool IsPaused() { return mAppPaused; }

	/**********************************************************
	*					Runtime functions
	**********************************************************/

	virtual void Draw() = 0;								// Function to execute draw calls
	virtual void Update() = 0;
public:
	int  Run();									// Main program cycle
protected:

	void FlushCommandQueue();					// Used to wait till GPU finishes execution
	virtual void OnResize();							// Called when user finishes resizing
	void CalculateFrameStats();					// Update window title with FPS

	// Mouse events
	virtual void OnMouseDown(WPARAM btnState, int x, int y) = 0;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) = 0;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) = 0;

	/**********************************************************
	*				Debug functions
	**********************************************************/

	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

};