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
#include <string>

#include "MathHelper.h"
#include "d3dUtil.h"
#include "d3dinit.h"

struct RenderItem
{
	RenderItem() = default;

	// World matrix for the shape to indicate its transform
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();

	// Number of frame resources to update
	int numFramesDirty = 3;

	// Index of the per object CB in the heap
	UINT ObjCBIndex = -1;

	// Associated MeshGeometry
	MeshGeometry* Geo = nullptr;

	// Primitive topology
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	std::string Submesh;
};
