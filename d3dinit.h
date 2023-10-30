/*****************************************************************//**
 * \file   d3dinit.h
 * \brief  Main header file, contains all variables for operating D3D.
 * 
 * \author Mikalai Varapai
 * \date   October 2023
 *********************************************************************/

#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include <string>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <memory>

#include "timer.h"
#include "window.h"
#include "d3dUtil.h"
#include "UploadBuffer.h"
#include "FrameResource.h"
#include "RenderItem.h"
#include "structures.h"

// Class that initializes and operates DirectX 12
class D3DApp
{

	/******************************************************
	 *					Initialization
	 ******************************************************/

public:

	// Get D3DApp instance
	static D3DApp* GetApp();

	// Initialize the window and DirectX
	static void Initialize(HINSTANCE hInst,				
		int nCmdShow, Timer* timer);

private:

	// Used to make this class one-instance, so
	// that it cannot be accessed from outside
	D3DApp(Timer* timer);

	// Forbid copying
	D3DApp(D3DApp& rhs) = delete;
	D3DApp& operator=(D3DApp& rhs) = delete;

private:

	/******************************************************
	 *					Class pointers
	 ******************************************************/

	// D3DApp static instance
	static D3DApp*										mApp;

	// Timer instance, passed in Main
	Timer*												mTimer = nullptr;

	// Debug interface
	Microsoft::WRL::ComPtr<ID3D12Debug>					mDebugController = nullptr;

	// Objects for creating other objects from D3D12 and DXGI
	Microsoft::WRL::ComPtr<IDXGIFactory4>				mdxgiFactory = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Device>				md3dDevice = nullptr;

	// Flush management
	Microsoft::WRL::ComPtr<ID3D12Fence>					mFence = nullptr;
	std::vector<std::unique_ptr<FrameResource>>			mFrameResources;
	FrameResource*										mCurrFrameResource = nullptr;

	// Command objects
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>			mCommandQueue = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>		mCommandAllocator = nullptr;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>	mCommandList = nullptr;

	// Arrays of descriptors
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		mRtvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		mDsvHeap = nullptr;

	// Instances of GPU resources
	static const int swapChainBufferCount = 2;
	Microsoft::WRL::ComPtr<IDXGISwapChain>				mSwapChain = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource>				mDepthStencilBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource>				mSwapChainBuffer[swapChainBufferCount];

	// Shader input signature
	std::vector<D3D12_INPUT_ELEMENT_DESC>				mInputLayout;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		mCbvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature>			mRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob>					mvsByteCode = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob>					mpsByteCode = nullptr;

	// Pipeline configuration
	Microsoft::WRL::ComPtr<ID3D12PipelineState>			mPSO = nullptr;

	// Geometry management
	std::unique_ptr<MeshGeometry>						mBoxGeo = nullptr;
	std::vector<std::unique_ptr<RenderItem>>			mAllRenderItems;

	// Viewport and scissor rect properties
	D3D12_VIEWPORT mViewport = { };
	D3D12_RECT mScissorRect = { };

	// Back buffer and DS buffer formats
	const DXGI_FORMAT mBackBufferFormat =				DXGI_FORMAT_R8G8B8A8_UNORM;
	const DXGI_FORMAT mDepthStencilFormat =				DXGI_FORMAT_D24_UNORM_S8_UINT;

	/******************************************************
	 *					Variables
	 ******************************************************/

private:

	// Constant buffer structure containing pass constants
	PassConstants mPassCB = { };

	UINT64 mCurrentFence = 0;
	int mCurrFrameResourceIndex = 0;
	int	mCurrBackBuffer = 0;

	// Current matrices
	DirectX::XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	// Parameters of matrices
	float mTheta = 1.5f * DirectX::XM_PI;
	float mPhi = DirectX::XM_PIDIV4;
	float mRadius = 5.0f;

	POINT mLastMousePos = { };

	// Descriptor sizes
	UINT mRtvDescriptorSize = 0;
	UINT mCbvSrvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;

private:
	int msaaQualityLevels = 0;
	bool msaaEnabled = false;

	// Window state members
	std::wstring mMainWindowCaption = L"Learning DirectX12";

	// Window management variables
	bool mAppPaused = false;
	bool mMaximized = false;
	bool mMinimized = false;
	bool mResizing = false;		// Used to terminate drawing while resizing

public:

	int gNumObjects = 1;
	static const int gNumFrameResources = 3;

private:

	// Initial window dimensions, changed per onResize() call
	UINT mClientWidth = 800;
	UINT mClientHeight = 600;

/**********************************************************
 *				Initialization functions
 **********************************************************/
public:
	
	void InitD3D();								// Create DirectX objects
	LRESULT MsgProc(HWND hwnd, UINT msg,		// Function for message processing,
		WPARAM wParam, LPARAM lParam);			// called from WndProc of D3DWindow

private:

	void CreateDevice();						// Create D3D12Device and factory
	void CreateFenceAndQueryDescriptorSizes();	// Create ID3D12Fence and get descriptor sizes
	void CreateCommandObjects();				// Create command queue, allocator, list
	void CreateSwapChain();						// Create IDXGISwapChain, along with two buffers
	void CreateRTVAndDSVDescriptorHeaps();		// Create arrays of descriptors

	void BuildFrameResources();					// Creates frame resource containers

	void CreateConstantBufferHeap();			// Create shader-visible CBV heap
	void BuildConstantBuffers();				// Creates per object and pass CBVs
	void BuildRootSignature();					// Defines the shader input signature
	void BuildShadersAndInputLayout();			// Compiles shaders and defines input layout
	void BuildBoxGeometry();					// Builds MeshGeometry of the box and creates RenderItem
	void BuildPSO();							// Configures rendering pipeline


	// Used in OnResize():
	void CreateRenderTargetView();				// Create descriptors for swap chain buffers
	void CreateDepthStencilBufferAndView();		// Create depth/stencil buffer and descriptor

	/**********************************************************
	*				Getter functions
	**********************************************************/
private:
	
	int GetMSAAQualityLevels();					// Get max quality levels for 4X MSAA
	float AspectRatio();						// Get ratio of width to height
	
	// Get current descriptor handles
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

	// Get current GPU handles for CBVs
	D3D12_GPU_DESCRIPTOR_HANDLE GetPassCBV(UINT frameResouceIndex) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetPerObjectCBV(UINT frameResouceIndex, UINT objectIndex) const;

	// Get current back buffer resource
	ID3D12Resource* GetCurrentBackBuffer();
public:
	bool IsPaused();							// Get paused state

	/**********************************************************
	*					Runtime functions
	**********************************************************/

public:
	int  Run();									// Main program cycle
private:
	void Draw();								// Function to execute draw calls
	void DrawRenderItems();						// Draw every render item

	void Update();								// Function for game logic
	void UpdateCamera();						// Update view matrix
	void UpdatePassCB();						// Update and store in CB pass constants
	void UpdateObjectCBs();						// Update and store in CB object's world matrix

	void FlushCommandQueue();					// Used to wait till GPU finishes execution
	void OnResize();							// Called when user finishes resizing
	void CalculateFrameStats();					// Update window title with FPS

	// Mouse events
	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);

	/**********************************************************
	*				Debug functions
	**********************************************************/

	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

};