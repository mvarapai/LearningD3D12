
#include <dxgi1_4.h>
#include <wrl.h>

#include "d3dcomponent.h"
#include "d3dresource.h"
#include "d3dUtil.h"

using Microsoft::WRL::ComPtr;

namespace D3DHelper
{
	void EnableDebugInterface(ID3D12Debug** ppDebug)
	{
		ThrowIfFailed(D3D12GetDebugInterface(
			IID_PPV_ARGS(ppDebug)));
		(*ppDebug)->EnableDebugLayer();
	}

	void CreateDevice(IDXGIFactory4** ppFactory, ID3D12Device** ppDevice)
	{
		// Create dxgi factory
		ThrowIfFailed(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, 
			IID_PPV_ARGS(ppFactory)));

		// Try to create hardware device
		HRESULT hardwareResult = D3D12CreateDevice(
			nullptr,									// Default adapter
			D3D_FEATURE_LEVEL_11_0,						// Min feature level
			IID_PPV_ARGS(ppDevice));					// Pointer to device

		// Fallback to WARP device
		if (FAILED(hardwareResult))
		{
			ComPtr<IDXGIAdapter> pWarpAdapter;
			ThrowIfFailed((*ppFactory)->EnumWarpAdapter
			(IID_PPV_ARGS(pWarpAdapter.GetAddressOf())));

			// Try again with WARP adapter
			ThrowIfFailed(D3D12CreateDevice(pWarpAdapter.Get(),
				D3D_FEATURE_LEVEL_11_0,
				IID_PPV_ARGS(ppDevice)));
		}
	}

	void CreateFence(ID3D12Device* pDevice, ID3D12Fence** ppFence)
	{
		ThrowIfFailed(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(ppFence)));
	}

	void CreateCommandObjects(ID3D12Device* pDevice,
		ID3D12GraphicsCommandList** ppCommandList,
		ID3D12CommandAllocator** ppCommandAllocator,
		ID3D12CommandQueue** ppCommandQueue)
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = { };
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

		ThrowIfFailed(pDevice->CreateCommandQueue(&queueDesc,
			IID_PPV_ARGS(ppCommandQueue)));

		// Create command allocator

		ThrowIfFailed(pDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(ppCommandAllocator)));

		// Create command list for that allocator

		ThrowIfFailed(pDevice->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			*ppCommandAllocator,
			nullptr,
			IID_PPV_ARGS(ppCommandList)));

		// Close the command list
		(*ppCommandList)->Close();
	}

	void CreateSwapChain(UINT width, UINT height, DXGI_FORMAT format,
		UINT bufferCount, HWND window, bool windowed,
		IDXGIFactory4* pFactory, ID3D12CommandQueue* pCommandQueue,
		IDXGISwapChain** ppSwapChain)
	{
		*ppSwapChain = nullptr;

		DXGI_SWAP_CHAIN_DESC sd = { };
		sd.BufferDesc.Width = width;
		sd.BufferDesc.Height = height;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferDesc.Format = format;
		sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = bufferCount;
		sd.OutputWindow = window;
		sd.Windowed = windowed;
		sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		ThrowIfFailed(pFactory->CreateSwapChain(
			pCommandQueue,
			&sd,
			ppSwapChain));
	}

	void CreateRTVAndDSVDescriptorHeaps(UINT bufferCount,
		ID3D12Device* pDevice,
		ID3D12DescriptorHeap** ppRTVHeap,
		ID3D12DescriptorHeap** ppDSVHeap)
	{
		// Create render target view heap, containing two views
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = { };
		rtvHeapDesc.NumDescriptors = bufferCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		rtvHeapDesc.NodeMask = 0;

		ThrowIfFailed(pDevice->CreateDescriptorHeap(
			&rtvHeapDesc, IID_PPV_ARGS(ppRTVHeap)));

		// Create depth/stencil view heap, containing one descriptor
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = { };
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		dsvHeapDesc.NodeMask = 0;

		ThrowIfFailed(pDevice->CreateDescriptorHeap(
			&dsvHeapDesc, IID_PPV_ARGS(ppDSVHeap)));
	}

	void CreateRTVs(ID3D12DescriptorHeap* pRTVHeap,
		UINT bufferCount, IDXGISwapChain* pSwapChain,
		ID3D12Device* pDevice,
		Microsoft::WRL::ComPtr<ID3D12Resource> ppBufferArray[2])
	{
		// Get pointer to the start of RTV heap
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle =
			pRTVHeap->GetCPUDescriptorHandleForHeapStart();

		SIZE_T rtvDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// Iterate through buffers
		for (UINT i = 0; i < bufferCount; i++)
		{
			// Store the i'th swap chain buffer in the array
			ThrowIfFailed(pSwapChain->GetBuffer(
				i, IID_PPV_ARGS(ppBufferArray[i].GetAddressOf())));

			// Create a descriptor to the i'th buffer
			pDevice->CreateRenderTargetView(
				ppBufferArray[i].Get(), nullptr, rtvHeapHandle);

			// Offset the descriptor pointer to its size for the next entry
			rtvHeapHandle.ptr += rtvDescriptorSize;
		}
	}

	void CreateDepthStencilBufferAndView(UINT width, UINT height,
		DXGI_FORMAT format, ID3D12DescriptorHeap* pDSVHeap,
		ID3D12GraphicsCommandList* pCommandList,
		ID3D12Device* pDevice, ID3D12Resource** ppDepthStencilBuffer)
	{
		D3D12_RESOURCE_DESC depthStencilDesc = { };
		depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthStencilDesc.Alignment = 0;
		depthStencilDesc.Width = width;
		depthStencilDesc.Height = height;
		depthStencilDesc.DepthOrArraySize = 1;
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.Format = format;
		depthStencilDesc.SampleDesc.Count = 1;
		depthStencilDesc.SampleDesc.Quality = 0;
		depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE optClear = { };
		optClear.Format = format;
		optClear.DepthStencil.Depth = 1.0f;
		optClear.DepthStencil.Stencil = 0;

		const D3D12_HEAP_PROPERTIES hp = HeapProperties(D3D12_HEAP_TYPE_DEFAULT);

		// Create resource and commit it to GPU

		ThrowIfFailed(pDevice->CreateCommittedResource(
			&hp,
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			D3D12_RESOURCE_STATE_COMMON,
			&optClear,
			IID_PPV_ARGS(ppDepthStencilBuffer)));

		// Create a view and place it in descriptor heap

		pDevice->CreateDepthStencilView(
			*ppDepthStencilBuffer,
			nullptr,
			pDSVHeap->GetCPUDescriptorHandleForHeapStart());

		// Transition the resource to write depth info

		Transition(*ppDepthStencilBuffer,
			pCommandList,
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}

	void CreateDefaultRootSignature(ID3D12Device* pDevice, ID3D12RootSignature** ppRootSignature)
	{
		// Root parameter can be a table, root descriptor or root constants.
		D3D12_ROOT_PARAMETER slotRootParameters[4] = { };

		// Pass CBV will be bound to b0
		D3D12_ROOT_DESCRIPTOR perPassCBV = { };
		perPassCBV.RegisterSpace = 0;
		perPassCBV.ShaderRegister = 0;

		D3D12_ROOT_DESCRIPTOR perObjectCBV = { };
		perObjectCBV.RegisterSpace = 0;
		perObjectCBV.ShaderRegister = 1;

		D3D12_ROOT_DESCRIPTOR materialCBV = { };
		materialCBV.RegisterSpace = 0;
		materialCBV.ShaderRegister = 2;

		D3D12_ROOT_DESCRIPTOR_TABLE srvTable = { };

		D3D12_DESCRIPTOR_RANGE srvDescriptorRange = { };
		srvDescriptorRange.BaseShaderRegister = 0;
		srvDescriptorRange.NumDescriptors = NUM_TEXTURES;
		srvDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		srvDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvDescriptorRange.RegisterSpace = 0;

		srvTable.pDescriptorRanges = &srvDescriptorRange;
		srvTable.NumDescriptorRanges = 1;

		slotRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		slotRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		slotRootParameters[0].Descriptor = perPassCBV;

		slotRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		slotRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		slotRootParameters[1].Descriptor = perObjectCBV;

		slotRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		slotRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		slotRootParameters[2].Descriptor = materialCBV;

		slotRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		slotRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		slotRootParameters[3].DescriptorTable = srvTable;

		// Create static samplers

		D3D12_STATIC_SAMPLER_DESC samplerDesc = { };
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = 1;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		samplerDesc.RegisterSpace = 0;
		samplerDesc.ShaderRegister = 0;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		// Create root signature description
		D3D12_ROOT_SIGNATURE_DESC rootDesc = { };
		rootDesc.NumParameters = _countof(slotRootParameters);
		rootDesc.pParameters = slotRootParameters;
		rootDesc.NumStaticSamplers = 1;
		rootDesc.pStaticSamplers = &samplerDesc;
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
		ThrowIfFailed(pDevice->CreateRootSignature(0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(ppRootSignature)));
	}

	D3D12_RASTERIZER_DESC DefaultRasterizerDesc()
	{
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

		return rd;
	}

	D3D12_BLEND_DESC DefaultBlendDescOff()
	{
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

		return bd;
	}

	D3D12_DEPTH_STENCIL_DESC DefaultDepthStencilDesc()
	{
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

		return dsd;
	}
}
