#pragma once
#include <d3d12.h>
#include <DirectXCollision.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <memory>
#include <string>

namespace Carol
{
	class Heap;
	class DescriptorAllocator;
	class DisplayManager;
	class Timer;
	class Camera;
	class GlobalResources;

	class BaseRenderer
	{
	public:
		BaseRenderer(HWND hWnd, uint32_t width, uint32_t height);
		BaseRenderer(const BaseRenderer&) = delete;
		BaseRenderer(BaseRenderer&&) = delete;
		BaseRenderer& operator=(const BaseRenderer&) = delete;

		virtual void CalcFrameState();
		virtual void Update() = 0;
		virtual void Draw() = 0;
		virtual void Tick();
		virtual void Stop();
		virtual void Start();

		virtual void OnMouseDown(WPARAM btnState, int x, int y) = 0;
		virtual void OnMouseUp(WPARAM btnState, int x, int y) = 0;
		virtual void OnMouseMove(WPARAM btnState, int x, int y) = 0;
		virtual void OnKeyboardInput() = 0;
		virtual void OnResize(uint32_t width, uint32_t height);
		
		virtual void SetPaused(bool state);
		virtual bool Paused();
		virtual void SetMaximized(bool state);
		virtual bool Maximized();
		virtual void SetMinimized(bool state);
		virtual bool Minimized();
		virtual void SetResizing(bool state);
		virtual bool Resizing();

	protected:
		float AspectRatio();

		virtual void InitTimer();
		virtual void InitCamera();

		virtual void InitDebug();
		virtual void InitDxgiFactory();
		virtual void InitDevice();
		virtual void InitFence();

		virtual void InitCommandQueue();
		virtual void InitCommandAllocator();
		virtual void InitCommandList();

		virtual void InitHeaps();
		virtual void InitAllocators();
		virtual void InitDisplay();

		virtual void FlushCommandQueue();

	protected:
		Microsoft::WRL::ComPtr<ID3D12Debug> mDebugLayer;
		Microsoft::WRL::ComPtr<IDXGIFactory> mDxgiFactory;
		Microsoft::WRL::ComPtr<ID3D12Device> mDevice;
		Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
		uint32_t mCpuFence;

		Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mInitCommandAllocator;

		std::unique_ptr<Heap> mDefaultBuffersHeap;
        std::unique_ptr<Heap> mUploadBuffersHeap;
        std::unique_ptr<Heap> mReadbackBuffersHeap;
		std::unique_ptr<Heap> mTexturesHeap;

		std::unique_ptr<DescriptorAllocator> mCbvSrvUavAllocator;
		std::unique_ptr<DescriptorAllocator> mRtvAllocator;
		std::unique_ptr<DescriptorAllocator> mDsvAllocator;

		std::unique_ptr<DisplayManager> mDisplay;

		std::unique_ptr<Timer> mTimer;
		std::unique_ptr<Camera> mCamera;

		D3D12_VIEWPORT mScreenViewport;
		D3D12_RECT mScissorRect;

		HWND mhWnd;
		std::wstring mMainWndCaption = L"Carol";
		D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;

		uint32_t mClientWidth = 800;
		uint32_t mClientHeight = 600;		
		DirectX::XMINT2 mLastMousePos;

		std::unique_ptr<GlobalResources> mGlobalResources;

		bool mPaused = false;
		bool mMaximized = false;
		bool mMinimized = false;
		bool mResizing = false;
	};
}