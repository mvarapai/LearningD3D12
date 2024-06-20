/**********************************************************************
 * \file   DefaultDrawable.h
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

/**
 * Interface that defines any object that can be drawn.
 * 
 * VB and IB are set by static function of the children.
 */
class IDrawable
{
protected:
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	SubmeshGeometry Submesh;

	IDrawable(D3D12_PRIMITIVE_TOPOLOGY topology, SubmeshGeometry submesh) : 
		PrimitiveTopology(topology), Submesh(submesh) { }

public:
	
	static void SetVBAndIB(ID3D12GraphicsCommandList* pCommandList,
		const D3D12_VERTEX_BUFFER_VIEW &vbv,
		const D3D12_INDEX_BUFFER_VIEW &ibv)
	{
		pCommandList->IASetVertexBuffers(0, 1, &vbv);
		pCommandList->IASetIndexBuffer(&ibv);
	}

	/**
	 * Call only after SetVBAndIB.
	 * 
	 * \param pCmdList Command List
	 * \param pCurrentFrameResource Current FrameResource
	 */
	void Draw(ID3D12GraphicsCommandList* pCmdList,
		FrameResource* pCurrentFrameResource)
	{
		pCmdList->IASetPrimitiveTopology(PrimitiveTopology);

		SetRootParameters(pCmdList, pCurrentFrameResource);

		pCmdList->DrawIndexedInstanced(Submesh.IndexCount, 1,
			Submesh.StartIndexLocation, Submesh.BaseVertexLocation, 0);
	}

protected:
	virtual void SetRootParameters(ID3D12GraphicsCommandList* pCmdList,
		FrameResource* pCurrentFrameResource) = 0;
};

/**
 * Implementation for IDrawable interface.
 * Uses default PSO. VB and IB are set manually
 */
class DefaultDrawable : public IDrawable
{
public:
	DefaultDrawable(const SubmeshGeometry& submesh,
		UINT objectCBIndex, UINT materialCBIndex,
		D3D12_GPU_DESCRIPTOR_HANDLE textureDescriptorHandle,
		D3D12_PRIMITIVE_TOPOLOGY primitiveTypology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST) : 

		IDrawable(primitiveTypology, submesh),
		ObjectCBIndex(objectCBIndex),
		MaterialCBIndex(materialCBIndex),
		TextureHandle(textureDescriptorHandle)
	{	
	}

	DefaultDrawable& operator=(DefaultDrawable& rhs) = delete;

	void SetRootParameters(ID3D12GraphicsCommandList* pCmdList,
		FrameResource* pCurrentFrameResource) override
	{
		// Set the CB descriptor to the 1 slot of descriptor table
		pCmdList->SetGraphicsRootConstantBufferView(1, 
			pCurrentFrameResource->ObjectCB->GetGPUHandle(ObjectCBIndex));
		pCmdList->SetGraphicsRootConstantBufferView(2, 
			pCurrentFrameResource->MaterialCB->GetGPUHandle(MaterialCBIndex));
		pCmdList->SetGraphicsRootDescriptorTable(3, TextureHandle);
	}
private:
	UINT ObjectCBIndex = 0;
	UINT MaterialCBIndex = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE TextureHandle;
};

