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

// Initilalize static variable with nullptr
D3DApp* D3DApp::mApp = nullptr;

// Constructor: sets static instance of the class and the timer
D3DApp::D3DApp(Timer* timer)
{
	mApp = this;
	mTimer = timer;
}

// Getter for the static D3DApp instance
D3DApp* D3DApp::GetApp() { return mApp; }

/**
* Getter function for paused state.
* @return True if the application is paused,
* false otherwise
*/
bool D3DApp::IsPaused() { return mAppPaused; }

/**
* Function that returns the ratio of window width to height.
* @return Aspect ratio
*/
float D3DApp::AspectRatio()
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}

/**
* Create and intialize DirectX and the game window.
* Sets the timer before the creation of a window so that
* first WndProc call can have access to it.
* 
* @param hInst - window instance
* @param nCmdShow - window show command
* @param timer - pointer to Time class instance
*/
void D3DApp::Initialize(HINSTANCE hInst, int nCmdShow, Timer* timer)
{
	new D3DApp(timer);
	D3DWindow::CreateD3DWindow(hInst, nCmdShow);
	mApp->InitD3D();
}

/// Calls D3D initialization functions
void D3DApp::InitD3D()
{
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

	CreateConstantBufferHeap();
	BuildConstantBuffers();
	BuildRootSignature();
	BuildShadersAndInputLayout();
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
void D3DApp::CreateDevice()
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
void D3DApp::CreateFenceAndQueryDescriptorSizes()
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
void D3DApp::CreateCommandObjects()
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
void D3DApp::CreateSwapChain()
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
	sd.OutputWindow = D3DWindow::GetWindow()->GetWindowHandle();
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ThrowIfFailed(mdxgiFactory->CreateSwapChain(
		mCommandQueue.Get(),
		&sd,
		mSwapChain.GetAddressOf()));
}

// Create descriptor heaps - RTV and DSV
void D3DApp::CreateRTVAndDSVDescriptorHeaps()
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
void D3DApp::CreateRenderTargetView()
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
void D3DApp::CreateDepthStencilBufferAndView()
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
void D3DApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; i++)
	{
		mFrameResources.push_back(
			std::make_unique<FrameResource>(md3dDevice.Get(), 1, gNumObjects));
	}
	mCamera = std::make_unique<Camera>(XMVectorSet(5.0f, 2.0f, 5.0f, 1.0f), XM_PI * 7 / 4, -0.2f, mTimer);
}

void D3DApp::BuildGeometry()
{
	// Initialize MeshGeometry
	mMeshGeometry = std::make_unique<MeshGeometry<Vertex>>(md3dDevice.Get(), mCommandList.Get());

	std::unique_ptr<RenderItem> renderItem1 = std::make_unique<RenderItem>(
		RenderItem::CreatePaintedCube(mMeshGeometry.get(), 0));

	std::unique_ptr<RenderItem> renderItem2 = std::make_unique<RenderItem>(
		RenderItem::CreateGrid(mMeshGeometry.get(), 1, 10, 1.0f));

	std::unique_ptr<RenderItem> terrain = std::make_unique<RenderItem>(
		RenderItem::CreateTerrain(mMeshGeometry.get(), 2, 100, 100, 100.0f, 100.0f));

	// Create GPU resources
	mMeshGeometry->ConstructMeshGeometry();

	// Initialize render item arrays
	mAllRenderItems.push_back(std::vector<std::unique_ptr<RenderItem>>());
	mAllRenderItems.push_back(std::vector<std::unique_ptr<RenderItem>>());

	mAllRenderItems[0].push_back(std::move(renderItem1));
	mAllRenderItems[0].push_back(std::move(terrain));
	mAllRenderItems[1].push_back(std::move(renderItem2));

}

// Returns amount of quality levels available for 4X MSAA
int D3DApp::GetMSAAQualityLevels()
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
D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView() const
{
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

// Get current back buffer as a GPU resource
ID3D12Resource* D3DApp::GetCurrentBackBuffer()
{
	return mSwapChainBuffer[mCurrBackBuffer].Get();
}

// Get CPU handle for current render target view
D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView() const
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += (SIZE_T) mRtvDescriptorSize * mCurrBackBuffer;
	return handle;
}

// Wait till GPU finishes with all commands
void D3DApp::FlushCommandQueue()
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
void D3DApp::OnResize()
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
void D3DApp::CalculateFrameStats()
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

		SetWindowText(D3DWindow::GetWindow()->GetWindowHandle(), windowText.c_str());

		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

// Print debug string containing the list of adapters
void D3DApp::LogAdapters()
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
void D3DApp::LogAdapterOutputs(IDXGIAdapter* adapter)
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
void D3DApp::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
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
