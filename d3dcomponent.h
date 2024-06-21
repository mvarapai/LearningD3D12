/*****************************************************************//**
 * \file   d3dcomponent.h
 * \brief  File with helper functions for DirectX
 * 
 * \author Mikalai Varapai
 * \date   June 2024
 *********************************************************************/

#pragma once

#include <d3d12.h>

namespace D3DHelper
{
	void EnableDebugInterface(ID3D12Debug** ppDebug);
	void CreateDevice(IDXGIFactory4** ppFactory, ID3D12Device** ppDevice);
	void CreateFence(ID3D12Device* pDevice, ID3D12Fence** ppFence);
	void CreateCommandObjects(ID3D12Device* pDevice,
		ID3D12GraphicsCommandList** ppCommandList,
		ID3D12CommandAllocator** ppCommandAllocator,
		ID3D12CommandQueue** ppCommandQueue);
	void CreateSwapChain(UINT width, UINT height, DXGI_FORMAT format,
		UINT bufferCount, HWND window, bool windowed,
		IDXGIFactory4* pFactory, ID3D12CommandQueue* pCommandQueue,
		IDXGISwapChain** ppSwapChain);
	void CreateRTVAndDSVDescriptorHeaps(UINT bufferCount,
		ID3D12Device* pDevice,
		ID3D12DescriptorHeap** ppRTVHeap,
		ID3D12DescriptorHeap** ppDSVHeap);
	void CreateRTVs(ID3D12DescriptorHeap* pRTVHeap,
		UINT bufferCount, IDXGISwapChain* pSwapChain,
		ID3D12Device* pDevice,
		Microsoft::WRL::ComPtr<ID3D12Resource> ppBufferArray[2]);
	void CreateDepthStencilBufferAndView(UINT width, UINT height,
		DXGI_FORMAT format, ID3D12DescriptorHeap* pDSVHeap,
		ID3D12GraphicsCommandList* pCommandList,
		ID3D12Device* pDevice, ID3D12Resource** ppDepthStencilBuffer);
	void CreateDefaultRootSignature(ID3D12Device* pDevice, ID3D12RootSignature** ppRootSignature);
	D3D12_RASTERIZER_DESC DefaultRasterizerDesc();
	D3D12_BLEND_DESC DefaultBlendDescOff();
	D3D12_DEPTH_STENCIL_DESC DefaultDepthStencilDesc();
}

class RenderTarget
{

public:
	RenderTarget() = default;


};
