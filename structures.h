#pragma once

#include <d3d12.h>
#include <DirectXMath.h>

#include "MathHelper.h"

// Structure describing vertex buffer element format
struct Vertex
{
	DirectX::XMFLOAT3 Pos;		// Position in non-homogeneous coordinates
	DirectX::XMFLOAT3 Normal;	// Vertex normal
	DirectX::XMFLOAT2 TexC;		// Texture coordinates
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
	float _pad0 = 0;

	DirectX::XMFLOAT4 AmbientLight = { };

	DirectX::XMFLOAT4 FogColor = { };

	float FogStart = 0;		// Distance at which objects start to fade
	float FogRange = 0;		// Distance at which objects are no longer visible
	float _pad1 = 0;
	float _pad2 = 0;

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

struct SubmeshGeometry
{
	UINT IndexCount = 0;            // How many indices to draw
	UINT StartIndexLocation = 0;    // From which to start
	INT BaseVertexLocation = 0;     // Padding of the indices
};

struct Shader
{
	std::vector<D3D12_INPUT_ELEMENT_DESC>				mInputLayout;
	Microsoft::WRL::ComPtr<ID3D12RootSignature>			mRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob>					mvsByteCode = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob>					mpsByteCode = nullptr;

	D3D12_INPUT_LAYOUT_DESC GetInputLayoutDesc()
	{
		return { mInputLayout.data(), (UINT)mInputLayout.size() };
	}

	D3D12_SHADER_BYTECODE GetVertexShader()
	{
		return {reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
			mvsByteCode->GetBufferSize() };
	}

	D3D12_SHADER_BYTECODE GetPixelShader()
	{
		return { reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
			mpsByteCode->GetBufferSize() };
	}

	void Set(D3D12_GRAPHICS_PIPELINE_STATE_DESC& psoDesc)
	{
		psoDesc.InputLayout = GetInputLayoutDesc();
		psoDesc.pRootSignature = mRootSignature.Get();
		psoDesc.VS = GetVertexShader();
		psoDesc.PS = GetPixelShader();
	}
};
