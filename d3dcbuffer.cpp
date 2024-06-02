/*****************************************************************//**
 * \file   d3dcbuffer.cpp
 * \brief  Contains implementations for d3d_base constant buffer methods.
 * 
 * \author Mikalai Varapai
 * \date   October 2023
 *********************************************************************/
#include "d3dinit.h"

// Create CBV heap with pass and per object CBVs and shader visible flag
void d3d_base::CreateSRVHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = { };
	srvHeapDesc.NumDescriptors = mNumTextures;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc,
		IID_PPV_ARGS(mSrvHeap.GetAddressOf())));
}

void d3d_base::BuildSRVs()
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = mSrvHeap->GetCPUDescriptorHandleForHeapStart();

	for (int i = 0; i < mNumTextures; i++)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = mTextures[i]->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = mTextures[i]->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		md3dDevice->CreateShaderResourceView(mTextures[i].Get(), &srvDesc, handle);

		handle.ptr += mCbvSrvDescriptorSize;
	}
}

// Returns GPU descriptor handle for current frame's pass CBV
D3D12_GPU_DESCRIPTOR_HANDLE d3d_base::GetTextureSRV(UINT textureIndex) const
{
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = mSrvHeap->GetGPUDescriptorHandleForHeapStart();
	textureHandle.ptr += static_cast<UINT64>(textureIndex) * mCbvSrvDescriptorSize;
	return textureHandle;
}