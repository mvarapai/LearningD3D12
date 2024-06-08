// **************************************************************************
//							UploadBuffer.h									*
//																			*
//	Wrapper class for creating a buffer resource on the upload heap,		*
//	so that it can be modified by the CPU during runtime.					*
//																			*
//	Constructor creates commited resource with upload heap and maps its		*
//	contents to the class member with pointer to the data.					*
//																			*
//	CopyData(int elementIndex, const T& data) - copies contents of T		*
//	to the [elementIndex] entry to the buffer, using CB padding if necessary*
//	Is used to copy only one element, for an array an overload can be used.	*
//																			*
// **************************************************************************

#pragma once

#include <wrl.h>

#include "d3dUtil.h"

/**
 * Class-wrapper for GPU upload buffer.
 * 
 * Usage:
 *	Create through constructor
 */
template<typename T>
class UploadBuffer
{
public:
	// Constructor for creating upload buffer
	UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer)
	{
		// Consider general non-CB case
		mElementByteSize = sizeof(T);

		// Constant buffer elements need to be a multiple of 256 bytes.
		// This is because the compiler can only view constant data
		// at m * 256 byte offsets and of n * 256 byte lengths.
		if (isConstantBuffer)
			mElementByteSize = CalcConstantBufferByteSize(sizeof(T));

		// Memory allocation info
		D3D12_HEAP_PROPERTIES hp = HeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		
		// Upload buffer description
		D3D12_RESOURCE_DESC bufferDesc = BufferDesc(static_cast<UINT64>(mElementByteSize) * elementCount);

		// Create upload buffer and commit it to the GPU heap
		ThrowIfFailed(device->CreateCommittedResource(
			&hp,								// Upload heap properties
			D3D12_HEAP_FLAG_NONE,				
			&bufferDesc,						// Resource description
			D3D12_RESOURCE_STATE_COMMON,		// Initial state
			nullptr,
			IID_PPV_ARGS(mUploadBuffer.GetAddressOf())));

		// Get pointer to the underlying memory
		// 0, nullptr means that we map the entire resource
		// mMappedData contains pointer to contents of the buffer,
		// where we can upload during runtime
		ThrowIfFailed(mUploadBuffer->Map(0, nullptr,
			reinterpret_cast<void**>(&mMappedData)));

		// We do not unmap until the destructor is called.
		// However, we must not write to the resource while it is in
		// use by the GPU, therefore we should use synchronization
	}

	// Forbid copying
	UploadBuffer(UploadBuffer& rhs) = delete;
	UploadBuffer& operator=(const UploadBuffer& rhs) = delete;

	// Unmap data upon destruction
	~UploadBuffer()
	{
		if (mUploadBuffer != nullptr)
		{
			mUploadBuffer->Unmap(0, nullptr);
		}
		mMappedData = nullptr;
	}

	// Getter for ID3D12Resource interface
	ID3D12Resource* Resource() const { return mUploadBuffer.Get(); }

	// Copy given element to the buffer at [elementIndex] slot
	void CopyData(int elementIndex, const T& data)
	{
		memcpy(&mMappedData[elementIndex * mElementByteSize],
			&data, sizeof(T));
	}

	// Convenience method to copy an array of data, including CB padding
	void CopyData(int firstElementIndex, int numElements, const T* dataArray)
	{
		for (int i = 0; i < numElements; i++)
		{
			CopyData(firstElementIndex + i, dataArray[i]);
		}
	}

	D3D12_GPU_VIRTUAL_ADDRESS GetGPUHandle(int index)
	{
		D3D12_GPU_VIRTUAL_ADDRESS base = mUploadBuffer->GetGPUVirtualAddress();
		base += mElementByteSize * index;
		return base;
	}

private:
	// Pointer to underlying GPU resource
	// Is released automatically
	Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer = nullptr;

	// Pointer to data contained in the buffer
	// It is mapped upon creation and unmapped when destroyed
	BYTE* mMappedData = nullptr;

	// Byte size of one element of an upload buffer
	// Is padded to multiple of 256 bytes in case of constant buffer
	UINT mElementByteSize = 0;

	// 
	bool mIsConstantBuffer = false;
};