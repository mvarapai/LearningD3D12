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
	BuildBoxGeometry();
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
			std::make_unique<FrameResource>(md3dDevice.Get(), 1, 1));
	}
}

// Create root signature
void D3DApp::BuildRootSignature()
{
	// Root parameter can be a table, root descriptor or root constants.
	D3D12_ROOT_PARAMETER slotRootParameters[2] = { };

	// Initialize first parameter root descriptor
	D3D12_DESCRIPTOR_RANGE passDescriptorTable = { };
	passDescriptorTable.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	passDescriptorTable.NumDescriptors = 1;
	passDescriptorTable.BaseShaderRegister = 0;
	passDescriptorTable.RegisterSpace = 0;
	passDescriptorTable.OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// Create a single range with only one descriptor at register 0
	D3D12_DESCRIPTOR_RANGE cbvTable = { };
	cbvTable.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	cbvTable.NumDescriptors = gNumObjects;
	cbvTable.BaseShaderRegister = 1;
	cbvTable.RegisterSpace = 0;
	cbvTable.OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// Configure root parameter
	slotRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	// Specify that the parameter is a descriptor table
	slotRootParameters[0].ParameterType
		= D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	// Supply ranges of the descriptor table
	slotRootParameters[0].DescriptorTable.pDescriptorRanges = &passDescriptorTable;
	slotRootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

	// Configure root parameter
	slotRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	// Specify that the parameter is a descriptor table
	slotRootParameters[1].ParameterType
		= D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	// Supply ranges of the descriptor table
	slotRootParameters[1].DescriptorTable.pDescriptorRanges = &cbvTable;
	slotRootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

	// Create root signature description
	D3D12_ROOT_SIGNATURE_DESC rootDesc = { };
	rootDesc.NumParameters = _countof(slotRootParameters);
	rootDesc.pParameters = slotRootParameters;
	rootDesc.NumStaticSamplers = 0;
	rootDesc.pStaticSamplers = nullptr;
	rootDesc.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// Create a root signature with a single slot which points to
	// a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),
		errorBlob.GetAddressOf());

	// Error handling
	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	// Create the root signature
	ThrowIfFailed(md3dDevice->CreateRootSignature(0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

// Compile shaders and create input layout
void D3DApp::BuildShadersAndInputLayout()
{
	HRESULT hr = S_OK;

	mvsByteCode = CompileShader(L"Shaders\\vertex.hlsl",
		nullptr, "VS", "vs_5_0");
	mpsByteCode = CompileShader(L"Shaders\\color.hlsl",
		nullptr, "PS", "ps_5_0");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, 
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};
}

void D3DApp::BuildBoxGeometry()
{
	std::array<Vertex, 8> vertices =
	{
		Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
		Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
		Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
		Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
		Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
	};

	std::array<std::uint16_t, 36> indices =
	{
		// front face
		0, 1, 2,
		0, 2, 3,

		// back face
		4, 6, 5,
		4, 7, 6,

		// left face
		4, 5, 1,
		4, 1, 0,

		// right face
		3, 2, 6,
		3, 6, 7,

		// top face
		1, 5, 6,
		1, 6, 2,

		// bottom face
		4, 0, 3,
		4, 3, 7
	};

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	mBoxGeo = std::make_unique<MeshGeometry>();
	mBoxGeo->Name = "boxGeo";

	// Load memory in blobs
	ThrowIfFailed(D3DCreateBlob(vbByteSize, 
		mBoxGeo->VertexBufferCPU.GetAddressOf()));
	CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(),
		vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, 
		mBoxGeo->IndexBufferCPU.GetAddressOf()));
	CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(),
		indices.data(), ibByteSize);

	// Create default vertex and index buffers and load data

	mBoxGeo->VertexBufferGPU = CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize,
		mBoxGeo->VertexBufferUploader);

	mBoxGeo->IndexBufferGPU = CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize,
		mBoxGeo->IndexBufferUploader);

	mBoxGeo->VertexByteStride = sizeof(Vertex); // One vertex offset
	mBoxGeo->VertexBufferByteSize = vbByteSize; // VB offset
	mBoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	mBoxGeo->IndexBufferByteSize = ibByteSize;  // IB offset

	SubmeshGeometry submesh;

	submesh.IndexCount = (UINT)indices.size();	// How mush indices to process
	submesh.StartIndexLocation = 0;				// From where to read indices
	submesh.BaseVertexLocation = 0;				// Zero-indexed vertex

	mBoxGeo->DrawArgs["box"] = submesh;

	std::unique_ptr<RenderItem> renderItem = std::make_unique<RenderItem>();
	renderItem->Geo = mBoxGeo.get();
	renderItem->ObjCBIndex = 0;
	renderItem->Submesh = "box";

	mAllRenderItems.push_back(std::move(renderItem));
}

void D3DApp::BuildPSO()
{
	// Rasterizer desc

	D3D12_RASTERIZER_DESC rd = { };

	rd.FillMode = D3D12_FILL_MODE_SOLID; // Solid or wireframe
	rd.CullMode = D3D12_CULL_MODE_BACK;  // Cull the back of primitives
	rd.FrontCounterClockwise = FALSE;    // Clockwise vertices are front
	rd.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rd.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rd.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rd.DepthClipEnable = TRUE;
	rd.MultisampleEnable = FALSE;
	rd.AntialiasedLineEnable = FALSE;
	rd.ForcedSampleCount = 0;
	rd.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	// Blend desc
	D3D12_BLEND_DESC bd = { };
	bd.AlphaToCoverageEnable = FALSE;
	bd.IndependentBlendEnable = FALSE;
	const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
	{
		FALSE,FALSE,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_NOOP,
		D3D12_COLOR_WRITE_ENABLE_ALL,
	};
	for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		bd.RenderTarget[i] = defaultRenderTargetBlendDesc;

	// Depth/stencil description

	D3D12_DEPTH_STENCIL_DESC dsd = { };
	
	dsd.DepthEnable = TRUE;
	dsd.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	dsd.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	dsd.StencilEnable = FALSE;
	dsd.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	dsd.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
	{ D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
		D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
	dsd.FrontFace = defaultStencilOp;
	dsd.BackFace = defaultStencilOp;

	// Define the Pipeline State Object

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = { };
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
		mvsByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
		mpsByteCode->GetBufferSize()
	};
	psoDesc.RasterizerState = rd;
	psoDesc.BlendState = bd;
	psoDesc.DepthStencilState = dsd;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = msaaEnabled ? 4 : 1;
	psoDesc.SampleDesc.Quality = msaaEnabled ? (msaaQualityLevels - 1) : 0;
	psoDesc.DSVFormat = mDepthStencilFormat;
	
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
		&psoDesc, IID_PPV_ARGS(mPSO.GetAddressOf())));
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
