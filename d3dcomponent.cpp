
#include <dxgi1_4.h>
#include <wrl.h>

#include "d3dcomponent.h"
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
}
