/*********** FrameResource.h ****************
 *	Used to store constant buffer data		*
 *	and command allocator used to render	*
 *	associated frame.						*
 *											*
 *	To track usage of the frame resource,	*
 *	fence point is recorded.				*
 ********************************************/

#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl.h>
#include <memory>

#include "UploadBuffer.h"
#include "d3dUtil.h"
#include "structures.h"

struct FrameResource
{
public:
	// Constructor to create command allocator and initialize memory
	// for frame constant buffers
	FrameResource(ID3D12Device* pDevice, UINT passCount, UINT objCount)
	{
		pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(CommandListAllocator.GetAddressOf()));

		PassCB = std::make_unique<UploadBuffer<PassConstants>>(pDevice, passCount, true);
		ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(pDevice, objCount, true);
	}

	~FrameResource() { }
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;

	// We cannot reset command allocator until the GPU is done
	// processing the commands it stores, so each frame gets its own allocator.
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandListAllocator = nullptr;

	// Each frame has its own associated constant buffer resources, used
	// to render the scene.
	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

	// Fence value to mark commands up to this fence point. This lets us
	// check if the resource is still in use by the GPU.
	UINT64 Fence = 0;
};