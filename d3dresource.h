/*****************************************************************//**
 * \file   d3dresource.h
 * \brief  Describes static and dynamic resource structure
 * 
 * \author 20231063
 * \date   June 2024
 *********************************************************************/

#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <ResourceUploadBatch.h>
#include <DDSTextureLoader.h>

#include "structures.h"
#include "geometry.h"
#include "FrameResource.h"

#define NUM_OBJECTS 2
#define NUM_MATERIALS 2

#define NUM_TEXTURES 2
#define NUM_GEOMETRIES 1

#define NUM_FRAME_RESOURCES 3

struct GEOMETRY_DESCRIPTOR
{
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;

	std::vector<SubmeshGeometry> Submeshes;
};

class StaticResources
{
private:
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffers[NUM_GEOMETRIES];
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBuffers[NUM_GEOMETRIES];

	Microsoft::WRL::ComPtr<ID3D12Resource> Textures[NUM_TEXTURES];

public:
	GEOMETRY_DESCRIPTOR Geometries[NUM_GEOMETRIES];

public:

	void LoadGeometry(ID3D12Device* pDevice,
		ID3D12CommandQueue* pQueue,
		ID3D12Fence* pFence,
		UINT64& currentValue)
	{
		StaticGeometryUploader<Vertex> uploader(pDevice);

		CreateTerrain(&uploader, "Textures//heightmap.bmp");
		CreatePlane(&uploader, 100, 100, 128.0f, 128.0f);

		uploader.ConstructGeometry(VertexBuffers[0], IndexBuffers[0], pQueue, pFence, currentValue);

		Geometries[0].Submeshes = uploader.GetSubmeshes();
		Geometries[0].VertexBufferView = uploader.VertexBufferView();
		Geometries[0].IndexBufferView = uploader.IndexBufferView();
	}

	void LoadTextures(ID3D12Device* pDevice, ID3D12CommandQueue* pQueue)
	{
		DirectX::ResourceUploadBatch upload(pDevice);

		upload.Begin();

		DirectX::CreateDDSTextureFromFile(pDevice, upload, L"Textures\\grass.dds", Textures[0].GetAddressOf());
		DirectX::CreateDDSTextureFromFile(pDevice, upload, L"Textures\\water1.dds", Textures[1].GetAddressOf());

		auto finish = upload.End(pQueue);

		finish.wait();

		// Build and populate SRVs

		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = { };
		srvHeapDesc.NumDescriptors = NUM_TEXTURES;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		srvHeapDesc.NodeMask = 0;
		ThrowIfFailed(pDevice->CreateDescriptorHeap(&srvHeapDesc,
			IID_PPV_ARGS(mSRVHeap.GetAddressOf())));

		D3D12_CPU_DESCRIPTOR_HANDLE handle = mSRVHeap->GetCPUDescriptorHandleForHeapStart();
		cbvSrvDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		for (int i = 0; i < NUM_TEXTURES; i++)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = Textures[i]->GetDesc().Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = Textures[i]->GetDesc().MipLevels;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

			pDevice->CreateShaderResourceView(Textures[i].Get(), &srvDesc, handle);

			handle.ptr += cbvSrvDescriptorSize;
		}
	}

	// Returns GPU descriptor handle for current frame's pass CBV
	D3D12_GPU_DESCRIPTOR_HANDLE GetTextureSRV(UINT textureIndex) const
	{
		D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = mSRVHeap->GetGPUDescriptorHandleForHeapStart();
		textureHandle.ptr += static_cast<UINT64>(textureIndex) * cbvSrvDescriptorSize;
		return textureHandle;
	}

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSRVHeap = nullptr;
	SIZE_T cbvSrvDescriptorSize = 0;
};

struct ConstantBufferDataCPU
{
	ObjectConstants ObjectTransform[NUM_OBJECTS];
	int ObjectTransformModified[NUM_OBJECTS];		// Number dirty frames

	PassConstants PassBuffer = { };

	MaterialConstants Materials[NUM_MATERIALS];
	int MaterialModified[NUM_MATERIALS];			// Number dirty frames

	// Delete default and copy constuctors
	ConstantBufferDataCPU() = delete;
	ConstantBufferDataCPU(ConstantBufferDataCPU& other) = delete;
	ConstantBufferDataCPU& operator=(ConstantBufferDataCPU& rhs) = delete;

	// Initialize CPU memory
	ConstantBufferDataCPU(ObjectConstants* pTransformInitialData, MaterialConstants* pMaterialInitialData)
	{
		for (int i = 0; i < NUM_OBJECTS; i++)
		{
			ObjectTransform[i] = pTransformInitialData[i];
			ObjectTransformModified[i] = NUM_FRAME_RESOURCES;
		}

		for (int i = 0; i < NUM_MATERIALS; i++)
		{
			Materials[i] = pMaterialInitialData[i];
			MaterialModified[i] = NUM_FRAME_RESOURCES;
		}
	}
};

class DynamicResources
{
	std::unique_ptr<FrameResource> pFrameResources[NUM_FRAME_RESOURCES];
	UINT currFrameResourceIndex = 0;

	ConstantBufferDataCPU CBDataCPU;
public:
	FrameResource* pCurrentFrameResource = nullptr;

	DynamicResources(ID3D12Device* pDevice, 
		ObjectConstants* pTransformInitialData, MaterialConstants* pMaterialInitialData)
		: CBDataCPU(pTransformInitialData, pMaterialInitialData)
	{
		for (int i = 0; i < NUM_FRAME_RESOURCES; i++)
		{
			pFrameResources[i] =
				std::make_unique<FrameResource>(pDevice, 1, NUM_OBJECTS, NUM_MATERIALS);
		}
		pCurrentFrameResource = pFrameResources[currFrameResourceIndex].get();
	}

	void NextFrameResource(ID3D12Fence* pFence)
	{
		// Write to next frame resource
		currFrameResourceIndex =
			(currFrameResourceIndex + 1) % NUM_FRAME_RESOURCES;
		pCurrentFrameResource = pFrameResources[currFrameResourceIndex].get();

		// Check whether the GPU has finished processing current frame
		if (pCurrentFrameResource->Fence != 0
			&& pFence->GetCompletedValue() < pCurrentFrameResource->Fence)
		{
			HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
			ThrowIfFailed(pFence->SetEventOnCompletion(pCurrentFrameResource->Fence, eventHandle));
			if (eventHandle == nullptr) return;
			WaitForSingleObject(eventHandle, INFINITE);
			CloseHandle(eventHandle);
		}
	}

	void UpdateConstantBuffers()
	{
		// Update GPU buffer for object constants

		for (UINT i = 0; i < NUM_OBJECTS; i++)
		{
			// Copy only is modified
			if (CBDataCPU.ObjectTransformModified[i] > 0)
			{
				pCurrentFrameResource->ObjectCB->CopyData(i,
					CBDataCPU.ObjectTransform[i]);

				CBDataCPU.ObjectTransformModified[i]--;
			}
		}

		// Update GPU buffer for pass constants

		pCurrentFrameResource->PassCB->CopyData(0,
			CBDataCPU.PassBuffer);

		// Update GPU buffer for material constants

		for (UINT i = 0; i < NUM_MATERIALS; i++)
		{
			if (CBDataCPU.MaterialModified[i] > 0)
			{
				pCurrentFrameResource->MaterialCB->CopyData(i,
					CBDataCPU.Materials[i]);

				CBDataCPU.MaterialModified[i]--;
			}
		}
	}

	// Handles to retrieve and change CB data

	void SetObjectTransform(UINT index, const ObjectConstants& transform)
	{
		CBDataCPU.ObjectTransform[index] = transform;
		CBDataCPU.ObjectTransformModified[index] = NUM_FRAME_RESOURCES;
	}

	// We let application do pass constants assignment
	void SetPassConstants(const PassConstants& pass)
	{
		CBDataCPU.PassBuffer = pass;
	}

	void SetMaterial(UINT index, const MaterialConstants& material)
	{
		CBDataCPU.Materials[index] = material;
		CBDataCPU.MaterialModified[index] = NUM_FRAME_RESOURCES;
	}

	ObjectConstants GetTransform(UINT index) { return CBDataCPU.ObjectTransform[index]; }
	PassConstants GetPassConstants() { return CBDataCPU.PassBuffer; }
	MaterialConstants GetMaterialConstants(UINT index) { return CBDataCPU.Materials[index]; }

	D3D12_GPU_VIRTUAL_ADDRESS GetObjectCBDescriptor(UINT index) 
	{ return pCurrentFrameResource->ObjectCB->GetGPUHandle(index); }
	D3D12_GPU_VIRTUAL_ADDRESS GetPassCBDescriptor() 
	{ return pCurrentFrameResource->PassCB->GetGPUHandle(0); }
	D3D12_GPU_VIRTUAL_ADDRESS GetMaterialCBDescriptor(UINT index) 
	{ return pCurrentFrameResource->MaterialCB->GetGPUHandle(index); }
};
