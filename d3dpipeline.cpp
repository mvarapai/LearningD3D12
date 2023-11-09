/*****************************************************************//**
 * \file   d3dpipeline.cpp
 * \brief  Handles PSOs and shaders
 * 
 * \author 20231063
 * \date   November 2023
 *********************************************************************/

#include <d3d12.h>

#include "d3dinit.h"
#include "d3dUtil.h"

using Microsoft::WRL::ComPtr;

 // Create root signature
void D3DApp::BuildRootSignature()
{
	// Root parameter can be a table, root descriptor or root constants.
	D3D12_ROOT_PARAMETER slotRootParameters[2] = { };

	// Pass CBV will be bound to b0
	D3D12_ROOT_DESCRIPTOR perPassCBV = { };
	perPassCBV.RegisterSpace = 0;
	perPassCBV.ShaderRegister = 0;

	D3D12_ROOT_DESCRIPTOR perObjectCBV = { };
	perObjectCBV.RegisterSpace = 0;
	perObjectCBV.ShaderRegister = 1;

	slotRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	slotRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	slotRootParameters[0].Descriptor = perPassCBV;

	slotRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	slotRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	slotRootParameters[1].Descriptor = perObjectCBV;

	// Create root signature description
	D3D12_ROOT_SIGNATURE_DESC rootDesc = { };
	rootDesc.NumParameters = _countof(slotRootParameters);
	rootDesc.pParameters = slotRootParameters;
	rootDesc.NumStaticSamplers = 0;
	rootDesc.pStaticSamplers = nullptr;
	rootDesc.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// Create a root signature with a single slot which points to
	// a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),
		errorBlob.GetAddressOf());

	// Error handling
	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	// Create the root signature
	ThrowIfFailed(md3dDevice->CreateRootSignature(0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

// Compile shaders and create input layout
void D3DApp::BuildShadersAndInputLayout()
{
	HRESULT hr = S_OK;

	mvsByteCode = CompileShader(L"Shaders\\vertex.hlsl",
		nullptr, "VS", "vs_5_0");
	mpsByteCode = CompileShader(L"Shaders\\color.hlsl",
		nullptr, "PS", "ps_5_0");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};
}

void D3DApp::BuildPSO()
{
	// Rasterizer desc

	D3D12_RASTERIZER_DESC rd = { };

	rd.FillMode = D3D12_FILL_MODE_SOLID; // Solid or wireframe
	rd.CullMode = D3D12_CULL_MODE_BACK;  // Cull the back of primitives
	rd.FrontCounterClockwise = FALSE;    // Clockwise vertices are front
	rd.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rd.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rd.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rd.DepthClipEnable = TRUE;
	rd.MultisampleEnable = FALSE;
	rd.AntialiasedLineEnable = FALSE;
	rd.ForcedSampleCount = 0;
	rd.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	// Blend desc
	D3D12_BLEND_DESC bd = { };
	bd.AlphaToCoverageEnable = FALSE;
	bd.IndependentBlendEnable = FALSE;
	const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
	{
		FALSE,FALSE,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_NOOP,
		D3D12_COLOR_WRITE_ENABLE_ALL,
	};
	for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		bd.RenderTarget[i] = defaultRenderTargetBlendDesc;

	// Depth/stencil description

	D3D12_DEPTH_STENCIL_DESC dsd = { };

	dsd.DepthEnable = TRUE;
	dsd.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	dsd.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	dsd.StencilEnable = FALSE;
	dsd.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	dsd.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
	{ D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
		D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
	dsd.FrontFace = defaultStencilOp;
	dsd.BackFace = defaultStencilOp;

	// Define the Pipeline State Object

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = { };
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
		mvsByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
		mpsByteCode->GetBufferSize()
	};
	psoDesc.RasterizerState = rd;
	psoDesc.BlendState = bd;
	psoDesc.DepthStencilState = dsd;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = msaaEnabled ? 4 : 1;
	psoDesc.SampleDesc.Quality = msaaEnabled ? (msaaQualityLevels - 1) : 0;
	psoDesc.DSVFormat = mDepthStencilFormat;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
		&psoDesc, IID_PPV_ARGS(mPSOs[0].GetAddressOf())));

	// Create another PSO for line list drawing
	D3D12_GRAPHICS_PIPELINE_STATE_DESC linePSODesc = psoDesc;
	linePSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
		&linePSODesc, IID_PPV_ARGS(mPSOs[1].GetAddressOf())));
}