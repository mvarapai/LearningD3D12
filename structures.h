#pragma once

#include <d3d12.h>
#include <DirectXMath.h>

#include "MathHelper.h"

// Structure describing vertex buffer element format
struct Vertex
{
	DirectX::XMFLOAT3 Pos;	// Position in non-homogeneous coordinates
	DirectX::XMFLOAT4 Color; // RGBA color
};

struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
};

struct PassConstants
{
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
};