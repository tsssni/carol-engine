#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

namespace Carol
{
	class HeapManager;
    class HeapAllocInfo;
	class DescriptorManager;
	class TextureManager;
    class Shader;
	class RootSignature;
	class DisplayPass;
	class Timer;
	class Camera;
    class FrameConstants;
	class GlobalResources;

	class BaseRenderer
	{
	public:
		BaseRenderer(HWND hWnd);
		BaseRenderer(const BaseRenderer&) = delete;
		BaseRenderer(BaseRenderer&&) = delete;
		BaseRenderer& operator=(const BaseRenderer&) = delete;
		~BaseRenderer();

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
		virtual void OnResize(uint32_t width, uint32_t height, bool init = false);
		
		virtual void SetPaused(bool state);
		virtual bool IsPaused();
		virtual void SetMaximized(bool state);
		virtual bool IsZoomed();
		virtual void SetMinimized(bool state);
		virtual bool IsIconic();
		virtual void SetResizing(bool state);
		virtual bool IsResizing();

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

		virtual void InitHeapManager();
		virtual void InitDescriptorManager();
		virtual void InitTextureManager();

		virtual void InitRenderPass();
		virtual void InitDisplay();

		virtual void FlushCommandQueue();

	protected:
		Microsoft::WRL::ComPtr<ID3D12Debug> mDebugLayer;
		Microsoft::WRL::ComPtr<IDXGIFactory> mDxgiFactory;
		Microsoft::WRL::ComPtr<ID3D12Device> mDevice;

		Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;
		Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
		uint32_t mCpuFence = 0;

		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mInitCommandAllocator;
        std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> mFrameAllocator;
        std::vector<uint32_t> mGpuFence = { 0,0,0 };

		std::unique_ptr<DescriptorManager> mDescriptorManager;
		std::unique_ptr<HeapManager> mHeapManager;
		std::unique_ptr<TextureManager> mTextureManager;

		std::unique_ptr<DisplayPass> mDisplayPass;

		uint32_t mClientWidth = 0;
		uint32_t mClientHeight = 0;
		std::unique_ptr<Camera> mCamera;
		std::unique_ptr<Timer> mTimer;

		HWND mhWnd;
		std::wstring mMainWndCaption = L"Carol";
		D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;

		DirectX::XMINT2 mLastMousePos = { 0,0 };

		bool mPaused = false;
		bool mMaximized = false;
		bool mMinimized = false;
		bool mResizing = false;
	};
}