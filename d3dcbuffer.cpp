#include "d3dinit.h"

// Create CBV heap with pass and per object CBVs and shader visible flag
void D3DApp::CreateConstantBufferHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = { };
	// Slot 0 will be occupied by the root constants
	cbvHeapDesc.NumDescriptors = NumFrameResources + NumFrameResources * gNumObjects;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
		IID_PPV_ARGS(mCbvHeap.GetAddressOf())));
}

// Creates constant buffer views in the heap with the following layout:
// - first 3 elements are pass CBVs
// - next are per object CBVs, the CPU handle of each can be accessed with
// the following offset: 
//	= NumFrameResources		* mCbvSrvDescriptorSize
//  + frameResourceIndex	* (mCbvSrvDescriptorSize * gNumObjects)
//  + objectIndex			* mCbvSrvDescriptorSize
void D3DApp::BuildConstantBuffers()
{
	// Query constant buffer sizes
	UINT objCBByteSize =
		CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT passCBByteSize =
		CalcConstantBufferByteSize(sizeof(PassConstants));

	// Descriptor handle to write addresses
	// First 3 elements are for pass CB,
	// the next are for the per object CB,
	// having a stride of objCBByteSize * gNumObjects
	D3D12_CPU_DESCRIPTOR_HANDLE viewHeapAddress =
		mCbvHeap->GetCPUDescriptorHandleForHeapStart();

	// Set pass constants as first 3 elements
	for (int frameResourceIndex = 0; frameResourceIndex < NumFrameResources; frameResourceIndex++)
	{
		// Set the pass constant buffer view
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = { };
		cbvDesc.BufferLocation = mFrameResources[frameResourceIndex]->PassCB->Resource()->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = passCBByteSize;

		md3dDevice->CreateConstantBufferView(&cbvDesc, viewHeapAddress);

		// Offset the pointer by CBV size (is NOT equal to passCBByteSize!!!)
		viewHeapAddress.ptr += mCbvSrvDescriptorSize;
	}

	// By that point, CPU descriptor handle will point to the start of
	// per object constant buffer views, creating an offset of 3 * passCBByteSize

	// Then set object constants 
	for (int frameResourceIndex = 0; frameResourceIndex < NumFrameResources; frameResourceIndex++)
	{
		// Since now we are working with resource that stores
		// multiple objects, we will have to offset GPU virtual address

		D3D12_GPU_VIRTUAL_ADDRESS gpuObjAddress =
			mFrameResources[frameResourceIndex]->ObjectCB->Resource()->GetGPUVirtualAddress();

		// Iterate through all objects
		for (int objectIndex = 0; objectIndex < gNumObjects; objectIndex++)
		{
			// Set the per object constant buffer view
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = { };
			cbvDesc.BufferLocation = gpuObjAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			md3dDevice->CreateConstantBufferView(&cbvDesc, viewHeapAddress);

			// Offset the pointer by CB size
			viewHeapAddress.ptr += mCbvSrvDescriptorSize;
			gpuObjAddress += objCBByteSize;
		}
	}
}

// Returns GPU descriptor handle for current frame's pass CBV
D3D12_GPU_DESCRIPTOR_HANDLE D3DApp::GetPassCBV(UINT frameResouceIndex) const
{
	D3D12_GPU_DESCRIPTOR_HANDLE passHandle = mCbvHeap->GetGPUDescriptorHandleForHeapStart();
	passHandle.ptr += static_cast<UINT64>(frameResouceIndex) * mCbvSrvDescriptorSize;
	return passHandle;
}

// Returns GPU descriptor handle for per object CBV
D3D12_GPU_DESCRIPTOR_HANDLE D3DApp::GetPerObjectCBV(UINT frameResouceIndex, UINT objectIndex) const
{
	D3D12_GPU_DESCRIPTOR_HANDLE perObjectHandle = mCbvHeap->GetGPUDescriptorHandleForHeapStart();

	// Offset to the start of per object CBVs
	perObjectHandle.ptr += static_cast<UINT64>(NumFrameResources) * mCbvSrvDescriptorSize;

	// Offset to the start of current frame's per object CBVs
	perObjectHandle.ptr += static_cast<UINT64>(frameResouceIndex) * gNumObjects * mCbvSrvDescriptorSize;

	// Offset to the current object's CBV
	perObjectHandle.ptr += static_cast<UINT64>(objectIndex) * mCbvSrvDescriptorSize;

	return perObjectHandle;
}
