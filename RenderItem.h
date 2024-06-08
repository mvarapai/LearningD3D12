/**********************************************************************
 * \file   RenderItem.h
 * \brief  Defines a wrapper structure for geometry rendering
 * 
 * \author Mikalai Varapai
 * \date   October 2023
 *********************************************************************/
#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <string>
#include <vector>

#include "MathHelper.h"
#include "d3dUtil.h"
#include "geometry.h"
#include "structures.h"
#include "UploadBuffer.h"

#include "d3dresource.h"

class RenderItem
{
public:
	RenderItem(const SubmeshGeometry& submesh,
		UINT objectCBIndex, UINT materialCBIndex,
		D3D12_GPU_DESCRIPTOR_HANDLE textureDescriptorHandle,
		D3D12_PRIMITIVE_TOPOLOGY primitiveTypology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST) : 

		mSubmesh(submesh), 
		ObjectCBIndex(objectCBIndex),
		MaterialCBIndex(materialCBIndex),
		TextureHandle(textureDescriptorHandle),
		PrimitiveTopology(primitiveTypology)
	{	
	}

	RenderItem& operator=(RenderItem& rhs) = delete;

	void Draw(ID3D12GraphicsCommandList* pCmdList,
		FrameResource* pCurrentFrameResource)
	{
		pCmdList->IASetPrimitiveTopology(PrimitiveTopology);

		// Set the CB descriptor to the 1 slot of descriptor table
		pCmdList->SetGraphicsRootConstantBufferView(1, 
			pCurrentFrameResource->ObjectCB->GetGPUHandle(ObjectCBIndex));
		pCmdList->SetGraphicsRootConstantBufferView(2, 
			pCurrentFrameResource->MaterialCB->GetGPUHandle(MaterialCBIndex));
		pCmdList->SetGraphicsRootDescriptorTable(3, TextureHandle);

		pCmdList->DrawIndexedInstanced(mSubmesh.IndexCount, 1,
			mSubmesh.StartIndexLocation, mSubmesh.BaseVertexLocation, 0);
	}
private:
	// Primitive topology
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT ObjectCBIndex = 0;

	UINT MaterialCBIndex = 0;

	D3D12_GPU_DESCRIPTOR_HANDLE TextureHandle;
public:
	SubmeshGeometry mSubmesh;
};

