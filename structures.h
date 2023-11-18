#pragma once

#include <d3d12.h>
#include <DirectXMath.h>

#include "MathHelper.h"

// Structure describing vertex buffer element format
struct Vertex
{
	DirectX::XMFLOAT3 Pos;		// Position in non-homogeneous coordinates
	DirectX::XMFLOAT3 Normal;	// Vertex normal
};

struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
};

struct Light
{
	DirectX::XMFLOAT3 Strength; // Light color
	float FalloffStart; // point/spot light only
	DirectX::XMFLOAT3 Direction;// directional/spot light only
	float FalloffEnd; // point/spot light only
	DirectX::XMFLOAT3 Position; // point/spot light only
	float SpotPower; // spot light only
};

struct PassConstants
{
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT3 EyePosW = { };

	float NearZ = 0;
	float FarZ = 0;
	float TotalTime = 0;
	float DeltaTime = 0;

	DirectX::XMFLOAT4 AmbientLight = { };

	Light Lights[16];
};

struct MaterialConstants
{
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.25f;
	// Used in the chapter on texture mapping.
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

