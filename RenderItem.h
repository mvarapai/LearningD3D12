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

class RenderItem
{
public:
	RenderItem() = default;
	RenderItem& operator=(RenderItem& rhs) = delete;

	void Update(UploadBuffer<ObjectConstants>* constantBuffer)
	{
		// Update 'dirty' frames
		if (mNumFramesDirty > 0)
		{
			DirectX::XMMATRIX WorldMatrix = DirectX::XMLoadFloat4x4(&mWorld);
			ObjectConstants objConstants = { };
			DirectX::XMStoreFloat4x4(&objConstants.World, WorldMatrix);

			constantBuffer->CopyData(mObjCBIndex, objConstants);

			mNumFramesDirty--;
		}
	}

	void Draw(ID3D12GraphicsCommandList* pCmdList, D3D12_GPU_VIRTUAL_ADDRESS objectCBV, D3D12_GPU_VIRTUAL_ADDRESS materialCBV)
	{
		pCmdList->IASetPrimitiveTopology(mPrimitiveType);

		// Set the CB descriptor to the 1 slot of descriptor table
		pCmdList->SetGraphicsRootConstantBufferView(1, objectCBV);
		pCmdList->SetGraphicsRootConstantBufferView(2, materialCBV);

		pCmdList->DrawIndexedInstanced(mSubmesh.IndexCount, 1,
			mSubmesh.StartIndexLocation, mSubmesh.BaseVertexLocation, 0);
	}

	int GetCBIndex() { return mObjCBIndex; }

	int GetMaterialIndex() { return mMaterialIndex; }

	static RenderItem CreatePaintedCube(StaticGeometry<Vertex>* meshGeometry, UINT objCBIndex, UINT matIndex);
	static RenderItem CreateGrid(StaticGeometry<Vertex>* meshGeometry, UINT objCBIndex, UINT matIndex, UINT numRows, float cellLength);
	static RenderItem CreateTerrain(StaticGeometry<Vertex>* meshGeometry, UINT objCBIndex, UINT matIndex, UINT n, UINT m, float width, float depth);
	static RenderItem CreatePlane(StaticGeometry<Vertex>* meshGeometry, UINT objCBIndex, UINT matIndex, UINT n, UINT m, float width, float depth);

private:
	// World matrix for the shape to indicate its transform
	DirectX::XMFLOAT4X4 mWorld = MathHelper::Identity4x4();

	// Number of frame resources to update
	int mNumFramesDirty = 3;

	// Index of the per object CB in the heap
	UINT mObjCBIndex = -1;

	// Material of the render item
	int mMaterialIndex = -1;

	// Associated StaticGeometry
	StaticGeometry<Vertex>* mGeometry = nullptr;

	// Primitive topology
	D3D12_PRIMITIVE_TOPOLOGY mPrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
public:
	SubmeshGeometry mSubmesh;
};
