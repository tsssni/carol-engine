#pragma once
#include "DirectX/Display.h"
#include "DirectX/DescriptorHeap.h"
#include "Resource/GameTimer.h"
#include "Resource/Camera.h"
#include "Resource/Model.h"
#include "Resource/SkinnedData.h"
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
	class SkinnedModelInfo
	{
	public:
		SkinnedData* SkinnedInfo = nullptr;
		vector<DirectX::XMFLOAT4X4> FinalTransforms;
		wstring ClipName;

		int SkinnedCBIndex = 0;
		float TimePos = 0.0f;

		void UpdateSkinnedModel(float dt);
	};

	class RenderItem
	{
	public:
		DirectX::XMFLOAT4X4 World;
		DirectX::XMFLOAT4X4 HistWorld;
		DirectX::XMFLOAT4X4 TexTransform;

		Model* Model = nullptr;
		uint32_t BaseVertexLocation = 0;
		uint32_t StartIndexLocation = 0;
		uint32_t IndexCount;

		uint32_t ObjCBIndex;
		uint32_t SkinnedCBIndex;
		uint32_t MatTBIndex;

		int NumFramesDirty = 3;
	};

	class BaseRenderer
	{
	public:
		BaseRenderer() = default;
		BaseRenderer(const BaseRenderer&) = delete;
		BaseRenderer(BaseRenderer&&) = delete;
		BaseRenderer& operator=(const BaseRenderer&) = delete;

		virtual void InitRenderer(HWND hWnd, uint32_t width, uint32_t height);

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

		virtual void InitDebug();
		virtual void InitDxgiFactory();
		virtual void InitDevice();
		virtual void InitFence();

		virtual void InitCommandQueue();
		virtual void InitCommandAllocator();
		virtual void InitCommandList();

		virtual void InitRootSignature() = 0;
		virtual void InitRtvDsvDescriptorHeaps();
		virtual void InitCbvSrvUavDescriptorHeaps() = 0;
		virtual void InitShaders() = 0;
		virtual void InitModels() = 0;
		virtual void InitRenderItems() = 0;
		virtual void InitFrameResources() = 0;
		virtual void InitPSOs() = 0;

		virtual void FlushCommandQueue();

	protected:
		ComPtr<ID3D12Debug> mDebugLayer;
		ComPtr<IDXGIFactory> mDxgiFactory;
		ComPtr<ID3D12Device> mDevice;
		ComPtr<ID3D12Fence> mFence;
		uint64_t mCurrFence = 0;

		ComPtr<ID3D12CommandQueue> mCommandQueue;
		ComPtr<ID3D12GraphicsCommandList> mCommandList;
		ComPtr<ID3D12CommandAllocator> mInitCommandAllocator;
		unique_ptr<DisplayManager> mDisplayManager;

		unique_ptr<RtvDescriptorHeap> mRtvHeap;
		unique_ptr<DsvDescriptorHeap> mDsvHeap;
		unique_ptr<CbvSrvUavDescriptorHeap> mCbvSrvUavHeap;

		unique_ptr<GameTimer> mTimer;
		unique_ptr<Camera> mCamera;
		HWND mhWnd;

		D3D12_VIEWPORT mScreenViewport;
		D3D12_RECT mScissorRect;

		wstring mMainWndCaption = L"Carol";
		D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
		uint32_t mClientWidth = 800;
		uint32_t mClientHeight = 600;		

		DirectX::XMINT2 mLastMousePos;

		bool mPaused = false;
		bool mMaximized = false;
		bool mMinimized = false;
		bool mResizing = false;
	};
}