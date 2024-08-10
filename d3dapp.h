#pragma once

#include "d3dinit.h"

/**
 * Class that defines runtime behavior of the program.
 * 
 * Inherits from D3DBase, creates and maintains all other objects.
 */
class D3DApplication : public D3DBase
{
	std::unique_ptr<StaticResources>					pStaticResources = nullptr;
	std::unique_ptr<DynamicResources>					pDynamicResources = nullptr;

	Shader												mDefaultShader;

	// An array of pipeline states
	static const int									gNumRenderModes = 3;

	Microsoft::WRL::ComPtr<ID3D12PipelineState>			mDefaultPSO = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState>			mLinePSO = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState>			mBlendPSO = nullptr;

	std::unique_ptr<Camera>								mCamera = nullptr;

	std::unique_ptr<DefaultDrawable>					mTerrain = nullptr;
	std::unique_ptr<DefaultDrawable>					mWater = nullptr;

	DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();

private:
	void D3DBase::InitializeComponents() override
	{
		LoadResources();
		mCamera = std::make_unique<Camera>(DirectX::XMVectorSet(5.0f, 2.0f, 5.0f, 1.0f),
			DirectX::XM_PI * 7 / 4, -0.2f, mTimer.get());

		// temp
		D3DHelper::CreateDefaultRootSignature(md3dDevice.Get(), mDefaultShader.mRootSignature.GetAddressOf());

		BuildShadersAndInputLayout();
		BuildPSO();
	}

private:
	void LoadResources();
	void BuildShadersAndInputLayout();			// Compiles shaders and defines input layout
	void BuildPSO();							// Configures rendering pipeline

	void DrawRenderItems();						// Draw every render item

	void UpdatePassCB();						// Update and store in CB pass constants

	void Update() override;
	void Draw() override;
	void OnResize() override
	{
		D3DBase::OnResize();
		// Update/set projection matrix as it only depends on aspect ratio
		DirectX::XMMATRIX P = DirectX::XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi,
			AspectRatio(), 1.0f, 1000.0f);
		XMStoreFloat4x4(&mProj, P);
	}

	void OnMouseDown(WPARAM btnState, int x, int y) override;
	void OnMouseUp(WPARAM btnState, int x, int y) override;
	void OnMouseMove(WPARAM btnState, int x, int y) override;

};
