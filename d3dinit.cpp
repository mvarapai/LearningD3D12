#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

#include "window.h"
#include "d3dinit.h"
#include "d3dUtil.h"
#include "UploadBuffer.h"
#include "RenderItem.h"

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
	// Enable the debug layer
#if defined(DEBUG) || defined(_DEBUG)
	{	
		ThrowIfFailed(D3D12GetDebugInterface(
			IID_PPV_ARGS(mDebugController.GetAddressOf())));
		mDebugController->EnableDebugLayer();
	}
#endif

	// Create device and DXGI factory
	CreateDevice();

	// Create fence
	CreateFenceAndQueryDescriptorSizes();

	// Get information about quality levels

	msaaQualityLevels = GetMSAAQualityLevels();
	std::wstring text = L"***Quality levels: " + std::to_wstring(msaaQualityLevels) + L"\n";
	OutputDebugString(text.c_str());

	// Create command queue, list, allocator
	CreateCommandObjects();

	// Create back buffer resources in the swap chain
	CreateSwapChain();

	// Create descriptor heaps for RTV and DSV
	CreateRTVAndDSVDescriptorHeaps();
	
	// Do the initial resize
	// Back buffer array and DS buffers are reset and resized
	OnResize();		// Executes command list

	// Debug output
	LogAdapters();

	// Reset the command list
	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

	// Create other objects

	BuildFrameResources();

	LoadTextures();
	CreateSRVHeap();
	BuildSRVs();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildMaterials();
	BuildGeometry();
	BuildPSO();

	// Execute initialization commands
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	// Wait till all commands are executed
	FlushCommandQueue();
}

// Create D3D device and DXGI factory
void d3d_base::CreateDevice()
{
	// Create dxgi factory
	ThrowIfFailed(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(
		mdxgiFactory.GetAddressOf())));

	// Try to create hardware device
	HRESULT hardwareResult = D3D12CreateDevice(
		nullptr,									// Default adapter
		D3D_FEATURE_LEVEL_11_0,						// Min feature level
		IID_PPV_ARGS(md3dDevice.GetAddressOf()));	// Pointer to device
	
	// Fallback to WARP device
	if (FAILED(hardwareResult))
	{
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed(mdxgiFactory->EnumWarpAdapter
		(IID_PPV_ARGS(pWarpAdapter.GetAddressOf())));

		// Try again with WARP adapter
		ThrowIfFailed(D3D12CreateDevice(pWarpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(md3dDevice.GetAddressOf())));
	}
}

// Creates fence and gets descriptor increment sizes
void d3d_base::CreateFenceAndQueryDescriptorSizes()
{
	// Create Fence and set 0 as initial value
	ThrowIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(mFence.GetAddressOf())));

	// Query descriptor increment sizes
	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

}

// Create command queue, allocator, list
void d3d_base::CreateCommandObjects()
{
	// Create command queue

	D3D12_COMMAND_QUEUE_DESC queueDesc = { };
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	ThrowIfFailed(md3dDevice->CreateCommandQueue(&queueDesc,
		IID_PPV_ARGS(mCommandQueue.GetAddressOf())));

	// Create command allocator

	ThrowIfFailed(md3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(mCommandAllocator.GetAddressOf())));

	// Create command list for that allocator

	ThrowIfFailed(md3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		mCommandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(mCommandList.GetAddressOf())));

	// Close the command list
	mCommandList->Close();
}

// Create/recreate swap chain, potentially used to enable MSAA
void d3d_base::CreateSwapChain()
{
	// Release previous swap chain we will be recreating
	mSwapChain.Reset(); // Reset the pointer itself

	DXGI_SWAP_CHAIN_DESC sd = { };
	sd.BufferDesc.Width =	mClientWidth;
	sd.BufferDesc.Height = mClientHeight;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = mBackBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = msaaEnabled ? 4 : 1;
	sd.SampleDesc.Quality = msaaEnabled ? (msaaQualityLevels - 1) : 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = swapChainBufferCount;
	sd.OutputWindow = mhWnd;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ThrowIfFailed(mdxgiFactory->CreateSwapChain(
		mCommandQueue.Get(),
		&sd,
		mSwapChain.GetAddressOf()));
}

// Create descriptor heaps - RTV and DSV
void d3d_base::CreateRTVAndDSVDescriptorHeaps()
{
	// Create render target view heap, containing two views
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = { };
	rtvHeapDesc.NumDescriptors = swapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;

	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	// Create depth/stencil view heap, containing one descriptor
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = { };
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;

	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}

// Gets buffers from swap chain and creates views for them.
// Stores swap chain buffers in the array field.
void d3d_base::CreateRenderTargetView()
{
	// Get pointer to the start of RTV heap
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle =
		mRtvHeap->GetCPUDescriptorHandleForHeapStart();

	// Iterate through buffers
	for (UINT i = 0; i < swapChainBufferCount; i++)
	{
		// Store the i'th swap chain buffer in the array
		ThrowIfFailed(mSwapChain->GetBuffer(
			i, IID_PPV_ARGS(mSwapChainBuffer[i].GetAddressOf())));

		// Create a descriptor to the i'th buffer
		md3dDevice->CreateRenderTargetView(
			mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);

		// Offset the descriptor pointer to its size for the next entry
		rtvHeapHandle.ptr += (SIZE_T)mRtvDescriptorSize;
	}
}

// Create a DS resource, commit it to the GPU and create a descriptor
void d3d_base::CreateDepthStencilBufferAndView()
{
	D3D12_RESOURCE_DESC depthStencilDesc = { };
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = mClientWidth;
	depthStencilDesc.Height = mClientHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = mDepthStencilFormat;
	depthStencilDesc.SampleDesc.Count = msaaEnabled ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = 
		msaaEnabled ? (msaaQualityLevels - 1) : 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear = { };
	optClear.Format = mDepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	const D3D12_HEAP_PROPERTIES hp = HeapProperties(D3D12_HEAP_TYPE_DEFAULT);

	// Create resource and commit it to GPU

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&hp,
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));

	// Create a view and place it in descriptor heap

	md3dDevice->CreateDepthStencilView(
		mDepthStencilBuffer.Get(),
		nullptr,
		DepthStencilView());

	// Transition the resource to write depth info

	Transition(mDepthStencilBuffer.Get(),
		mCommandList.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

// Create frame resouce objects
void d3d_base::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; i++)
	{
		mFrameResources.push_back(
			std::make_unique<FrameResource>(md3dDevice.Get(), 1, gNumObjects, gNumMaterials));
	}
	mCamera = std::make_unique<Camera>(XMVectorSet(5.0f, 2.0f, 5.0f, 1.0f), XM_PI * 7 / 4, -0.2f, mTimer.get());
}

void d3d_base::BuildMaterials()
{
	auto grass = std::make_unique<Material>();
	grass->Name = "grass";
	grass->CBIndex = 0;
	grass->DiffuseAlbedo = XMFLOAT4(0.0f, 0.6f, 0.0f, 1.0f);
	grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	grass->Roughness = 1.0f;
	// This is not a good water material definition, but we do not have
	// all the rendering tools we need (transparency, environment
	// reflection), so we fake it for now.
	auto water = std::make_unique<Material>();
	water->Name = "water";
	water->CBIndex = 1;
	water->DiffuseAlbedo = XMFLOAT4(0.0f, 0.2f, 0.6f, 0.5f);
	water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	water->Roughness = 0.0f;
	mMaterials["grass"] = std::move(grass);
	mMaterials["water"] = std::move(water);
}

void d3d_base::BuildGeometry()
{
	// Initialize StaticGeometry
	mMeshGeometry = std::make_unique<StaticGeometry<Vertex>>(md3dDevice.Get(), mCommandList.Get());

	//std::unique_ptr<RenderItem> renderItem1 = std::make_unique<RenderItem>(
	//	RenderItem::CreatePaintedCube(mMeshGeometry.get(), 0, mMaterials["grass"]->CBIndex));

	//std::unique_ptr<RenderItem> renderItem2 = std::make_unique<RenderItem>(
	//	RenderItem::CreateGrid(mMeshGeometry.get(), 1, mMaterials["grass"]->CBIndex, 10, 1.0f));

	std::unique_ptr<RenderItem> terrain = std::make_unique<RenderItem>(
		RenderItem::CreateTerrain(mMeshGeometry.get(), 0, mMaterials["grass"]->CBIndex, 100, 100, 100.0f, 100.0f));
	terrain->mTextureIndex = 0;

	std::unique_ptr<RenderItem> plane = std::make_unique<RenderItem>(
		RenderItem::CreatePlane(mMeshGeometry.get(), 1, 1, 100, 100, 100.0f, 100.0f));

	// Create GPU resources
	mMeshGeometry->ConstructGeometry();

	// Initialize render item arrays
	mAllRenderItems.push_back(std::vector<std::unique_ptr<RenderItem>>());
	mAllRenderItems.push_back(std::vector<std::unique_ptr<RenderItem>>());
	mAllRenderItems.push_back(std::vector<std::unique_ptr<RenderItem>>());

	mAllRenderItems[0].push_back(std::move(terrain));
	mAllRenderItems[2].push_back(std::move(plane));

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
	handle.ptr += (SIZE_T) mRtvDescriptorSize * mCurrBackBuffer;
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

	CreateRenderTargetView();
	CreateDepthStencilBufferAndView();

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
