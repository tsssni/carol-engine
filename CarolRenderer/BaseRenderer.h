#pragma once
#include <d3d12.h>
#include <DirectXCollision.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <memory>
#include <string>

using Microsoft::WRL::ComPtr;
using std::unique_ptr;
using std::wstring;

namespace Carol
{
	class Heap;
	class DescriptorAllocator;
	class DisplayManager;
	class GameTimer;
	class Camera;
	class RenderData;

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
		ComPtr<ID3D12Debug> mDebugLayer;
		ComPtr<IDXGIFactory> mDxgiFactory;
		ComPtr<ID3D12Device> mDevice;
		ComPtr<ID3D12Fence> mFence;
		uint32_t mCpuFence;

		ComPtr<ID3D12CommandQueue> mCommandQueue;
		ComPtr<ID3D12GraphicsCommandList> mCommandList;
		ComPtr<ID3D12CommandAllocator> mInitCommandAllocator;

		unique_ptr<Heap> mDefaultBuffersHeap;
        unique_ptr<Heap> mUploadBuffersHeap;
        unique_ptr<Heap> mReadbackBuffersHeap;
        unique_ptr<Heap> mSrvTexturesHeap;
        unique_ptr<Heap> mRtvDsvTexturesHeap;

		unique_ptr<DescriptorAllocator> mCbvSrvUavAllocator;
		unique_ptr<DescriptorAllocator> mRtvAllocator;
		unique_ptr<DescriptorAllocator> mDsvAllocator;

		unique_ptr<DisplayManager> mDisplay;

		unique_ptr<GameTimer> mTimer;
		unique_ptr<Camera> mCamera;

		D3D12_VIEWPORT mScreenViewport;
		D3D12_RECT mScissorRect;

		HWND mhWnd;
		wstring mMainWndCaption = L"Carol";
		D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;

		uint32_t mClientWidth = 800;
		uint32_t mClientHeight = 600;		
		DirectX::XMINT2 mLastMousePos;

		unique_ptr<RenderData> mRenderData;

		bool mPaused = false;
		bool mMaximized = false;
		bool mMinimized = false;
		bool mResizing = false;
	};
}