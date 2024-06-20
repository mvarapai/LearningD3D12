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

class IDrawable
{
protected:
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	SubmeshGeometry Submesh;

	IDrawable(D3D12_PRIMITIVE_TOPOLOGY topology, SubmeshGeometry submesh) : 
		PrimitiveTopology(topology), Submesh(submesh) { }

public:
	virtual void Draw(ID3D12GraphicsCommandList* pCmdList,
		FrameResource* pCurrentFrameResource) = 0;
};

class RenderItem : IDrawable
{
public:
	RenderItem(const SubmeshGeometry& submesh,
		UINT objectCBIndex, UINT materialCBIndex,
		D3D12_GPU_DESCRIPTOR_HANDLE textureDescriptorHandle,
		D3D12_PRIMITIVE_TOPOLOGY primitiveTypology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST) : 

		IDrawable(primitiveTypology, submesh),
		ObjectCBIndex(objectCBIndex),
		MaterialCBIndex(materialCBIndex),
		TextureHandle(textureDescriptorHandle)
	{	
	}

	RenderItem& operator=(RenderItem& rhs) = delete;

	void Draw(ID3D12GraphicsCommandList* pCmdList,
		FrameResource* pCurrentFrameResource) override
	{
		pCmdList->IASetPrimitiveTopology(PrimitiveTopology);

		// Set the CB descriptor to the 1 slot of descriptor table
		pCmdList->SetGraphicsRootConstantBufferView(1, 
			pCurrentFrameResource->ObjectCB->GetGPUHandle(ObjectCBIndex));
		pCmdList->SetGraphicsRootConstantBufferView(2, 
			pCurrentFrameResource->MaterialCB->GetGPUHandle(MaterialCBIndex));
		pCmdList->SetGraphicsRootDescriptorTable(3, TextureHandle);

		pCmdList->DrawIndexedInstanced(Submesh.IndexCount, 1,
			Submesh.StartIndexLocation, Submesh.BaseVertexLocation, 0);
	}
private:
	UINT ObjectCBIndex = 0;
	UINT MaterialCBIndex = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE TextureHandle;
};

