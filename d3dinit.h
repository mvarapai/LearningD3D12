#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>

#include "timer.h"
#include "window.h"
#include <string>

// Function to create D3D objects

class D3DApp
{
public:
	// Get D3DApp instance
	static D3DApp* GetApp();

	// Custom message processing function, is called in WndProc
	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	D3DApp(Timer* timer);

private:

	// Used to make this class one-instance
	static D3DApp* mApp;

protected:
	// Timer instance, passed in Main
	Timer* mTimer = nullptr;

	// Objects for creating other objects from D3D12 and DXGI

	Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice = nullptr;

	// Used for flush

	UINT64 mCurrentFence = 0;
	Microsoft::WRL::ComPtr<ID3D12Fence> mFence = nullptr;

	// Command objects

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCommandAllocator = nullptr;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList = nullptr;

	// Contains 2 GPU resources

	Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain = nullptr;

	// Arrays of descriptors

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap = nullptr;

	// Instances of GPU resources

	static const int swapChainBufferCount = 2;
	int mCurrBackBuffer = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[swapChainBufferCount];

	// Structures containing window sizes

	D3D12_VIEWPORT mViewport = { };
	D3D12_RECT mScissorRect = { };

	// Descriptor sizes

	UINT mRtvDescriptorSize = 0;
	UINT mCbvSrvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;

	// Initialization functions
public:
	void Initialize(HINSTANCE hInst, int nCmdShow);
	void InitD3D(D3DWindow* window);					// Function called at initialization
private:
	inline void CreateDevice();							// Create D3D12Device and factory
	inline void CreateFenceAndQueryDescriptorSizes();	// Create ID3D12Fence and get descriptor sizes
	inline void CreateCommandObjects();					// Create command queue, allocator, list
	inline void CreateSwapChain();						// Create IDXGISwapChain, along with two buffers
	inline void CreateDescriptorHeaps();				// Create arrays of descriptors
	// Used in OnResize():
	inline void CreateRenderTargetView();				// Create descriptors for swap chain buffers
	inline void CreateDepthStencilBufferAndView();		// Create depth/stencil buffer and descriptor

	int GetMSAAQualityLevels();							// Get max quality levels for 4X MSAA

	// Get current descriptor handles

	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

	// Get current back buffer resource
public:
	ID3D12Resource* GetCurrentBackBuffer();
private:
	// Buffer formats

	const DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	const DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 4X MSAA

	int msaaQualityLevels = 0;
	bool msaaEnabled = false;

	// Window state members
	std::wstring mMainWindowCaption = L"Learning DirectX12";

	bool mAppPaused = false;		// If app is paused
	bool mMaximized = false;
	bool mMinimized = false;
	bool mResizing = false;			// Used to terminate drawing
									// while resizing

	UINT mClientWidth = 800;		// Window dimensions,
	UINT mClientHeight = 600;		// used for OnResize()

	// Process functions
public:
	// Defined in main
	void Draw();					// Function to execute draw calls
	void Update();					// Function for game logic
	int Run();						// Main program cycle
private:
	void FlushCommandQueue();		// Used to wait till GPU finishes execution
	void OnResize();				// Called when user finishes resizing
	void CalculateFrameStats();		// Update window title with FPS

	// Debug functions
	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

	// Mouse events
	void OnMouseDown(WPARAM btnState, int x, int y) { }
	void OnMouseUp(WPARAM btnState, int x, int y) { }
	void OnMouseMove(WPARAM btnState, int x, int y) { }
};