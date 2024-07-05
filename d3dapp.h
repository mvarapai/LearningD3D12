#pragma once

#include "d3dinit.h"

class D3DApplication : public d3d_base
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

	// Sort RenderItems by the PSO used to render them
	std::vector<std::unique_ptr<DefaultDrawable>>			mRenderItemsDefault;
	std::vector<std::unique_ptr<DefaultDrawable>>			mRenderItemsWireframe;

private:
	void d3d_base::InitializeComponents() override
	{

		LoadResources();
		mCamera = std::make_unique<Camera>(DirectX::XMVectorSet(5.0f, 2.0f, 5.0f, 1.0f),
			DirectX::XM_PI * 7 / 4, -0.2f, mTimer.get());
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

	void OnMouseDown(WPARAM btnState, int x, int y) override;
	void OnMouseUp(WPARAM btnState, int x, int y) override;
	void OnMouseMove(WPARAM btnState, int x, int y) override;

};
