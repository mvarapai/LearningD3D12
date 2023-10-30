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
#include "MathHelper.h"
#include "d3dUtil.h"
#include "UploadBuffer.h"
#include "FrameResource.h"
#include "RenderItem.h"

#include "structures.h"

// Class that initializes and operates DirectX 12
class D3DApp
{
public:
	// Get D3DApp instance
	static D3DApp* GetApp();
	// Initialize the window and DirectX
	static void Initialize(HINSTANCE hInst,				
		int nCmdShow, Timer* timer);
private:
	// Used to make this class one-instance
	static D3DApp* mApp;
	D3DApp(Timer* timer);
	D3DApp(D3DApp& rhs) = delete;
	D3DApp& operator=(D3DApp& rhs) = delete;
private:
	// Timer instance, passed in Main
	Timer* mTimer = nullptr;

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

	// Contains 2 GPU resources

	Microsoft::WRL::ComPtr<IDXGISwapChain>				mSwapChain = nullptr;

	// Arrays of descriptors

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		mRtvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>		mDsvHeap = nullptr;

	// Instances of GPU resources

	static const int swapChainBufferCount = 2;
	Microsoft::WRL::ComPtr<ID3D12Resource>				mDepthStencilBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource>				mSwapChainBuffer[swapChainBufferCount];

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

	// Contains ID3D12Resource instances
	std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;

	Microsoft::WRL::ComPtr<ID3DBlob> mvsByteCode = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> mpsByteCode = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<std::unique_ptr<RenderItem>> mAllRenderItems;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO = nullptr;

	D3D12_VIEWPORT mViewport = { };
	D3D12_RECT mScissorRect = { };

	const DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	const DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	/******************************************************
	 *					Variables
	 ******************************************************/
private:
	PassConstants mPassCB = { };

	UINT64 mCurrentFence = 0;
	int mCurrFrameResourceIndex = 0;
	int	mCurrBackBuffer = 0;

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

	bool mAppPaused = false;		// If app is paused
	bool mMaximized = false;
	bool mMinimized = false;
	bool mResizing = false;			// Used to terminate drawing while resizing

public:
	int gNumObjects = 1;
	static const int NumFrameResources = 3;
private:
	// Initial window dimensions
	UINT mClientWidth = 800;		// Window dimensions,
	UINT mClientHeight = 600;		// used for OnResize()

/**********************************************************
 *				Initialization functions
 **********************************************************/
public:
	
	void InitD3D();										// Create DirectX objects
	LRESULT MsgProc(HWND hwnd, UINT msg,				// Function for message processing,
		WPARAM wParam, LPARAM lParam);					// called from WndProc of D3DWindow

private:
	// Inline functions to separate 
	inline void CreateDevice();							// Create D3D12Device and factory
	inline void CreateFenceAndQueryDescriptorSizes();	// Create ID3D12Fence and get descriptor sizes
	inline void CreateCommandObjects();					// Create command queue, allocator, list
	inline void CreateSwapChain();						// Create IDXGISwapChain, along with two buffers
	inline void CreateRTVAndDSVDescriptorHeaps();		// Create arrays of descriptors

	void CreateConstantBufferHeap();					// Create shader-visible CBV heap
	void BuildConstantBuffers();						
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildBoxGeometry();
	void BuildPSO();

	void BuildFrameResources();

	// Used in OnResize():
	inline void CreateRenderTargetView();				// Create descriptors for swap chain buffers
	inline void CreateDepthStencilBufferAndView();		// Create depth/stencil buffer and descriptor

	int			GetMSAAQualityLevels();					// Get max quality levels for 4X MSAA
private:
	// Getters
	float AspectRatio();
	
	// Get current descriptor handles
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

	// Getters for CBVs
	D3D12_GPU_DESCRIPTOR_HANDLE GetPassCBV(UINT frameResouceIndex) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetPerObjectCBV(UINT frameResouceIndex, UINT objectIndex) const;

	// Get current back buffer resource
	ID3D12Resource* GetCurrentBackBuffer();
public:
	bool IsPaused();

public:
	// Process functions

	int  Run();						// Main program cycle
private:
	void Draw();					// Function to execute draw calls
	void DrawRenderItems();

	void Update();					// Function for game logic
	void UpdateCamera();
	void UpdatePassCB();
	void UpdateObjectCBs();

	void FlushCommandQueue();		// Used to wait till GPU finishes execution
	void OnResize();				// Called when user finishes resizing
	void CalculateFrameStats();		// Update window title with FPS

	// Debug functions
	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

	// Mouse events
	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);
};