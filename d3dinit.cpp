#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

#include "window.h"
#include "d3dinit.h"
#include "d3dUtil.h"
#include "UploadBuffer.h"
#include "drawable.h"
#include "d3dapp.h"


#include <windowsx.h>
#include <vector>
#include <array>
#include <DirectXColors.h>
#include <d3dcompiler.h>
#include <memory>

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

// Returns amount of quality levels available for 4X MSAA
int D3DBase::GetMSAAQualityLevels()
{
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels{};
	msQualityLevels.Format = mBackBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;

	ThrowIfFailed(md3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)));

	return msQualityLevels.NumQualityLevels;
}

void D3DApplication::LoadResources()
{
	// LOAD RESOURCES
	pStaticResources = std::make_unique<StaticResources>();
	pStaticResources->LoadGeometry(md3dDevice.Get(), mCommandQueue.Get(),
		mFence.Get(), mCurrentFence);
	pStaticResources->LoadTextures(md3dDevice.Get(), mCommandQueue.Get());

	// Set materials and transforms

	MaterialConstants materials[NUM_MATERIALS];
	// Grass
	materials[0].DiffuseAlbedo = DirectX::XMFLOAT4(0.0f, 0.6f, 0.0f, 1.0f);
	materials[0].FresnelR0 = DirectX::XMFLOAT3(0.01f, 0.01f, 0.01f);
	materials[0].Roughness = 0.8f;
	materials[0].MatTransform = MathHelper::Identity4x4();

	materials[1].DiffuseAlbedo = DirectX::XMFLOAT4(0.0f, 0.2f, 0.6f, 0.5f);
	materials[1].FresnelR0 = DirectX::XMFLOAT3(0.1f, 0.1f, 0.1f);
	materials[1].Roughness = 0.0f;
	materials[1].MatTransform = MathHelper::Identity4x4();

	ObjectConstants objects[NUM_OBJECTS];

	for (int i = 0; i < NUM_OBJECTS; i++)
	{
		objects[i].World = MathHelper::Identity4x4();
	}

	//XMMATRIX terrain = XMMatrixIdentity();
	//terrain *= XMMatrixTranslation(0.0f, -4.0f, 0.0f);
	//XMStoreFloat4x4(&objects[0].World, terrain);

	pDynamicResources = std::make_unique<DynamicResources>(md3dDevice.Get(), objects, materials);

	mTerrain = std::make_unique<DefaultDrawable>(
		pStaticResources->Geometries[0].Submeshes.at(0), 0, 0, pStaticResources->GetTextureSRV(0));

	mWater = std::make_unique<DefaultDrawable>(
		pStaticResources->Geometries[0].Submeshes.at(1), 1, 1, pStaticResources->GetTextureSRV(1));

}

// Calculate FPS and update window text
void D3DBase::CalculateFrameStats()
{
	// Make static variables so that they don't change
	// between function calls
	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Compute averages over 1 second period
	if ((mTimer->TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCnt;
		float mspf = 1000.0f / fps;

		std::wstring fpsStr = AnsiToWString(std::to_string(fps));
		std::wstring mspfStr = AnsiToWString(std::to_string(mspf));

		std::wstring windowText = mMainWindowCaption +
			L"		fps: " + fpsStr +
			L"		mspf: " + mspfStr;

		SetWindowText(mhWnd, windowText.c_str());

		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

// Print debug string containing the list of adapters
void D3DBase::LogAdapters()
{
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;

	while (mdxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC desc = { };
		adapter->GetDesc(&desc);

		std::wstring text = L"***Adapter: ";
		text += desc.Description;
		text += L"\n";

		OutputDebugString(text.c_str());

		adapterList.push_back(adapter);

		i++;
	}

	for (size_t i = 0; i < adapterList.size(); i++)
	{
		LogAdapterOutputs(adapterList[i]);
		adapterList[i]->Release();
	}
}

// For every adapter log a string of available outputs
void D3DBase::LogAdapterOutputs(IDXGIAdapter* adapter)
{
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC desc = { };
		output->GetDesc(&desc);

		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString(text.c_str());

		LogOutputDisplayModes(output, mBackBufferFormat);

		output->Release();

		i++;
	}
}

// For every output log a string of available display modes
void D3DBase::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
	UINT count = 0;
	UINT flags = 0;

	// Call with nullptr to get list count
	output->GetDisplayModeList(format, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modeList(count);
	output->GetDisplayModeList(format, flags, &count, &modeList[0]);

	for (DXGI_MODE_DESC& x : modeList)
	{
		UINT n = x.RefreshRate.Numerator;
		UINT d = x.RefreshRate.Denominator;
		std::wstring text =
			L"Width = " + std::to_wstring(x.Width) + L" " +
			L"Height = " + std::to_wstring(x.Height) + L" " +
			L"Refresh = " + std::to_wstring(n) + L"/" +
			std::to_wstring(d) + L"\n";

		OutputDebugString(text.c_str());
	}
}
