#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

#include "window.h"
#include "d3dinit.h"
#include "d3dUtil.h"
#include "UploadBuffer.h"

#include <windowsx.h>
#include <vector>
#include <array>
#include <DirectXColors.h>
#include <d3dcompiler.h>

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

// Get the instance of the D3DApp, or create one
D3DApp* D3DApp::GetApp() { return mApp; }

// Get paused state
bool D3DApp::IsPaused() { return mAppPaused; }

// Get aspect ratio
float D3DApp::AspectRatio()
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}

// Create window and initialize DirectX
void D3DApp::Initialize(HINSTANCE hInst, int nCmdShow, Timer* timer)
{
	new D3DApp(timer);
	D3DWindow::CreateD3DWindow(hInst, nCmdShow);
	mApp->InitD3D();
}

// Initialize DirectX
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

	CreateDevice();
	CreateFenceAndQueryDescriptorSizes();

	msaaQualityLevels = GetMSAAQualityLevels();
	//std::wstring text = L"***Quality levels: " + std::to_wstring(msaaQualityLevels) + L"\n";
	//OutputDebugString(text.c_str());

	CreateCommandObjects();
	CreateSwapChain();
	CreateDescriptorHeaps();
	
	// Do the initial resize
	OnResize();		// Executes command list
	//LogAdapters();

	// Reset the command list
	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

	// Create other objects

	BuildDescriptorHeaps();
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
inline void D3DApp::CreateDevice()
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
inline void D3DApp::CreateFenceAndQueryDescriptorSizes()
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
inline void D3DApp::CreateCommandObjects()
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
inline void D3DApp::CreateSwapChain()
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
inline void D3DApp::CreateDescriptorHeaps()
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

// Gets buffers from swap chain and creates views for them
// Called from OnResize() because it stores buffers
inline void D3DApp::CreateRenderTargetView()
{
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle =
		mRtvHeap->GetCPUDescriptorHandleForHeapStart();

	for (UINT i = 0; i < swapChainBufferCount; i++)
	{
		// Get pointer to the buffer resource
		ThrowIfFailed(mSwapChain->GetBuffer(
			i, IID_PPV_ARGS(mSwapChainBuffer[i].GetAddressOf())));

		// Create a descriptor to it
		md3dDevice->CreateRenderTargetView(
			mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);

		// Offset the descriptor handle
		rtvHeapHandle.ptr += (SIZE_T)mRtvDescriptorSize;
	}
}

// Create a DS object, commit it to the GPU and create a descriptor
inline void D3DApp::CreateDepthStencilBufferAndView()
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

// Create CBV heap with one element and shader visible flag
void D3DApp::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = { };
	cbvHeapDesc.NumDescriptors = 1;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
		IID_PPV_ARGS(mCbvHeap.GetAddressOf())));
}

void D3DApp::BuildConstantBuffers()
{
	mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>
		(md3dDevice.Get(), 1, true);

	UINT objCBByteSize =
		CalcConstantBufferByteSize(sizeof(ObjectConstants));

	D3D12_GPU_VIRTUAL_ADDRESS cbAddress =
		mObjectCB->Resource()->GetGPUVirtualAddress();

	int boxCBufIndex = 0;
	cbAddress += boxCBufIndex * objCBByteSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = { };
	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes = objCBByteSize;

	md3dDevice->CreateConstantBufferView(&cbvDesc,
		mCbvHeap->GetCPUDescriptorHandleForHeapStart());
}

// Create root signature
void D3DApp::BuildRootSignature()
{
	// Root parameter can be a table, root descriptor or root constants.
	D3D12_ROOT_PARAMETER slotRootParameter[1] = { };


	// Create a single range with only one descriptor at register 0
	D3D12_DESCRIPTOR_RANGE cbvTable = { };
	cbvTable.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	cbvTable.NumDescriptors = 1;
	cbvTable.BaseShaderRegister = 0;
	cbvTable.RegisterSpace = 0;
	cbvTable.OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// Configure root parameter
	slotRootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	// Specify that the parameter is a descriptor table
	slotRootParameter[0].ParameterType
		= D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	// Supply ranges of the descriptor table
	slotRootParameter[0].DescriptorTable.pDescriptorRanges = &cbvTable;
	slotRootParameter[0].DescriptorTable.NumDescriptorRanges = 1;

	// Create root signature description
	D3D12_ROOT_SIGNATURE_DESC rootDesc = { };
	rootDesc.NumParameters = 1;
	rootDesc.pParameters = slotRootParameter;
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
// Due to changes in screen dimensions applies new aspect ratio.
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
