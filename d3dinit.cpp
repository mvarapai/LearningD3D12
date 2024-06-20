#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

#include "window.h"
#include "d3dinit.h"
#include "d3dUtil.h"
#include "UploadBuffer.h"
#include "drawable.h"

#include "d3dcomponent.h"

#include <windowsx.h>
#include <vector>
#include <array>
#include <DirectXColors.h>
#include <d3dcompiler.h>
#include <memory>

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

/**
* Getter function for paused state.
* @return True if the application is paused,
* false otherwise
*/
bool d3d_base::IsPaused() { return mAppPaused; }

/**
* Function that returns the ratio of window width to height.
* @return Aspect ratio
*/
float d3d_base::AspectRatio()
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}

/// Calls D3D initialization functions
void d3d_base::Initialize(HWND hWnd)
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

	// Query descriptor increment sizes
	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_DSV);


	D3DHelper::CreateSwapChain(mClientWidth, mClientHeight,
		mBackBufferFormat, 2, mhWnd, true, mdxgiFactory.Get(),
		mCommandQueue.Get(), mSwapChain.GetAddressOf());

	D3DHelper::CreateRTVAndDSVDescriptorHeaps(2, md3dDevice.Get(),
		mRtvHeap.GetAddressOf(), mDsvHeap.GetAddressOf());

	// Do the initial resize
	// Back buffer array and DS buffers are reset and resized
	OnResize();		// Executes command list

	// Debug output
	LogAdapters();

	// Reset the command list
	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

	// LOAD RESOURCES
	pStaticResources = std::make_unique<StaticResources>();
	pStaticResources->LoadGeometry(md3dDevice.Get(), mCommandQueue.Get(),
		mFence.Get(), mCurrentFence);
	pStaticResources->LoadTextures(md3dDevice.Get(), mCommandQueue.Get());

	// Set materials and transforms

	MaterialConstants materials[NUM_MATERIALS];
	materials[0].DiffuseAlbedo = DirectX::XMFLOAT4(0.0f, 0.6f, 0.0f, 1.0f);
	materials[0].FresnelR0 = DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f);
	materials[0].Roughness = 1.0f;
	materials[0].MatTransform = MathHelper::Identity4x4();

	materials[1].DiffuseAlbedo = DirectX::XMFLOAT4(0.0f, 0.2f, 0.6f, 0.5f);
	materials[1].FresnelR0 = DirectX::XMFLOAT3(0.1f, 0.1f, 0.1f);
	materials[1].Roughness = 0.0f;
	materials[1].MatTransform = MathHelper::Identity4x4();

	ObjectConstants objects[NUM_OBJECTS];
	
	for (int i = 0; i < NUM_OBJECTS; i++)
	{
		objects[i].World = MathHelper::Identity4x4();
	}

	pDynamicResources = std::make_unique<DynamicResources>(md3dDevice.Get(), objects, materials);

	mRenderItemsDefault.push_back(std::make_unique<DefaultDrawable>(
		pStaticResources->Geometries[0].Submeshes.at(0), 0, 0, pStaticResources->GetTextureSRV(0)));

	mRenderItemsDefault.push_back(std::make_unique<DefaultDrawable>(
		pStaticResources->Geometries[0].Submeshes.at(1), 1, 1, pStaticResources->GetTextureSRV(1)));

	// Create other objects

	mCamera = std::make_unique<Camera>(XMVectorSet(5.0f, 2.0f, 5.0f, 1.0f), XM_PI * 7 / 4, -0.2f, mTimer.get());

	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildPSO();

	// Execute initialization commands
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	// Wait till all commands are executed
	FlushCommandQueue();
}

// Returns amount of quality levels available for 4X MSAA
int d3d_base::GetMSAAQualityLevels()
{
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels{};
	msQualityLevels.Format = mBackBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;

	ThrowIfFailed(md3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)));

	return msQualityLevels.NumQualityLevels;
}

// Get CPU handle for DS descriptor
D3D12_CPU_DESCRIPTOR_HANDLE d3d_base::DepthStencilView() const
{
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

// Get current back buffer as a GPU resource
ID3D12Resource* d3d_base::GetCurrentBackBuffer()
{
	return mSwapChainBuffer[mCurrBackBuffer].Get();
}

// Get CPU handle for current render target view
D3D12_CPU_DESCRIPTOR_HANDLE d3d_base::CurrentBackBufferView() const
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += (SIZE_T)mRtvDescriptorSize * mCurrBackBuffer;
	return handle;
}


// Wait till GPU finishes with all commands
void d3d_base::FlushCommandQueue()
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
void d3d_base::OnResize()
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

	// Update/set projection matrix as it only depends on aspect ratio

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi,
		AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

// Calculate FPS and update window text
void d3d_base::CalculateFrameStats()
{
	// Make static variables so that they don't change
	// between function calls
	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Compute averages over 1 second period
	if ((mTimer->TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCnt;
		float mspf = 1000.0f / fps;

		std::wstring fpsStr = AnsiToWString(std::to_string(fps));
		std::wstring mspfStr = AnsiToWString(std::to_string(mspf));

		std::wstring windowText = mMainWindowCaption +
			L"		fps: " + fpsStr +
			L"		mspf: " + mspfStr;

		SetWindowText(mhWnd, windowText.c_str());

		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

// Print debug string containing the list of adapters
void d3d_base::LogAdapters()
{
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;

	while (mdxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC desc = { };
		adapter->GetDesc(&desc);

		std::wstring text = L"***Adapter: ";
		text += desc.Description;
		text += L"\n";

		OutputDebugString(text.c_str());

		adapterList.push_back(adapter);

		i++;
	}

	for (size_t i = 0; i < adapterList.size(); i++)
	{
		LogAdapterOutputs(adapterList[i]);
		adapterList[i]->Release();
	}
}

// For every adapter log a string of available outputs
void d3d_base::LogAdapterOutputs(IDXGIAdapter* adapter)
{
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC desc = { };
		output->GetDesc(&desc);

		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString(text.c_str());

		LogOutputDisplayModes(output, mBackBufferFormat);

		output->Release();

		i++;
	}
}

// For every output log a string of available display modes
void d3d_base::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
	UINT count = 0;
	UINT flags = 0;

	// Call with nullptr to get list count
	output->GetDisplayModeList(format, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modeList(count);
	output->GetDisplayModeList(format, flags, &count, &modeList[0]);

	for (DXGI_MODE_DESC& x : modeList)
	{
		UINT n = x.RefreshRate.Numerator;
		UINT d = x.RefreshRate.Denominator;
		std::wstring text =
			L"Width = " + std::to_wstring(x.Width) + L" " +
			L"Height = " + std::to_wstring(x.Height) + L" " +
			L"Refresh = " + std::to_wstring(n) + L"/" +
			std::to_wstring(d) + L"\n";

		OutputDebugString(text.c_str());
	}
}
