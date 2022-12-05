#include "BaseRenderer.h"
#include "Manager/Display/Display.h"
#include "DirectX/DescriptorAllocator.h"
#include "DirectX/Heap.h"
#include "Resource/GameTimer.h"
#include "Resource/Camera.h"
#include "Resource/SkinnedData.h"
#include "Utils/Common.h"
#include <DirectXMath.h>
#include <memory>

using std::make_unique;

float Carol::BaseRenderer::AspectRatio()
{
	return 1.0f * mClientWidth / mClientHeight;
}

void Carol::BaseRenderer::InitTimer()
{
	mTimer = make_unique<GameTimer>();
	mTimer->Reset();
	mRenderData->Timer = mTimer.get();
}

void Carol::BaseRenderer::InitCamera()
{
	mCamera = make_unique<Camera>();
	mCamera->SetPosition(0.0f, 5.0f, -5.0f);
	mRenderData->Camera = mCamera.get();
}

Carol::BaseRenderer::BaseRenderer(HWND hWnd, uint32_t width, uint32_t height)
	:mhWnd(hWnd),mRenderData(make_unique<RenderData>())
{
	InitTimer();
	InitCamera();

#if defined(DEBUG) || defined(_DEBUG)
	InitDebug();
#endif
	InitDxgiFactory();
	InitDevice();
	InitFence();

	InitCommandAllocator();
	InitCommandList();
	InitCommandQueue();

	InitHeaps();
	InitAllocators();	
	InitDisplay();

	BaseRenderer::OnResize(width, height);
}

void Carol::BaseRenderer::CalcFrameState()
{
	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Compute averages over one second period.
	if ((mTimer->TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCnt; // fps = frameCnt / 1
		float mspf = 1000.0f / fps;

		wstring fpsStr = to_wstring(fps);
		wstring mspfStr = to_wstring(mspf);

		wstring windowText = mMainWndCaption +
			L"    fps: " + fpsStr +
			L"   mspf: " + mspfStr;

		SetWindowText(mhWnd, windowText.c_str());

		// Reset for next average.
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

void Carol::BaseRenderer::InitDebug()
{
	ComPtr<ID3D12Debug5> debugLayer;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(debugLayer.GetAddressOf())));
	debugLayer->EnableDebugLayer();
	debugLayer->SetEnableAutoName(true);

	mDebugLayer = debugLayer;
}

void Carol::BaseRenderer::InitDxgiFactory()
{
	ComPtr<IDXGIFactory4> dxgiFactory;
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory.GetAddressOf())));
	mDxgiFactory = dxgiFactory;
}

void Carol::BaseRenderer::InitDevice()
{
	ComPtr<ID3D12Device> device;
	ThrowIfFailed(D3D12CreateDevice(device.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(device.GetAddressOf())));
	mDevice = device;
	mRenderData->Device = mDevice.Get();
}

void Carol::BaseRenderer::InitFence()
{
	ComPtr<ID3D12Fence> fence;
	ThrowIfFailed(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())));
	mFence = fence;
}

void Carol::BaseRenderer::InitCommandQueue()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	ThrowIfFailed(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(mCommandQueue.GetAddressOf())));
	mRenderData->CommandQueue = mCommandQueue.Get();
}

void Carol::BaseRenderer::InitCommandAllocator()
{
	ThrowIfFailed(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mInitCommandAllocator.GetAddressOf())));
}

void Carol::BaseRenderer::InitCommandList()
{
	ThrowIfFailed(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mInitCommandAllocator.Get(), nullptr, IID_PPV_ARGS(mCommandList.GetAddressOf())));
	mCommandList->Close();
	mRenderData->CommandList = mCommandList.Get();
}

void Carol::BaseRenderer::InitHeaps()
{
	mDefaultBuffersHeap = make_unique<BuddyHeap>(mDevice.Get(), D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS, 1 << 29);
	mUploadBuffersHeap = make_unique<BuddyHeap>(mDevice.Get(), D3D12_HEAP_TYPE_UPLOAD, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS, 1 << 29);
	mReadbackBuffersHeap = make_unique<BuddyHeap>(mDevice.Get(), D3D12_HEAP_TYPE_READBACK, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
	mSrvTexturesHeap = make_unique<SegListHeap>(mDevice.Get(), D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES);
	mRtvDsvTexturesHeap = make_unique<SegListHeap>(mDevice.Get(), D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES);

	mRenderData->DefaultBuffersHeap = mDefaultBuffersHeap.get();
	mRenderData->UploadBuffersHeap = mUploadBuffersHeap.get();
	mRenderData->ReadbackBuffersHeap = mReadbackBuffersHeap.get();
	mRenderData->SrvTexturesHeap = mSrvTexturesHeap.get();
	mRenderData->RtvDsvTexturesHeap = mRtvDsvTexturesHeap.get();
}

void Carol::BaseRenderer::InitAllocators()
{
	mCbvSrvUavAllocator = make_unique<DescriptorAllocator>(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	mRtvAllocator = make_unique<DescriptorAllocator>(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvAllocator = make_unique<DescriptorAllocator>(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	mRenderData->CbvSrvUavAllocator = mCbvSrvUavAllocator.get();
	mRenderData->RtvAllocator = mRtvAllocator.get();
	mRenderData->DsvAllocator = mDsvAllocator.get();
}

void Carol::BaseRenderer::InitDisplay()
{
	mRenderData->ScreenViewport = &mScreenViewport;
	mRenderData->ScissorRect = &mScissorRect;
	
	mRenderData->ClientWidth = &mClientWidth;
	mRenderData->ClientHeight = &mClientHeight;

	mDisplay=make_unique<DisplayManager>(mRenderData.get(), mhWnd, mDxgiFactory.Get(), mClientWidth, mClientHeight, 2);
	mRenderData->Display = mDisplay.get();
}

void Carol::BaseRenderer::Tick()
{
	mTimer->Tick();
}

void Carol::BaseRenderer::Stop()
{
	mTimer->Stop();
}

void Carol::BaseRenderer::Start()
{
	mTimer->Start();
}

void Carol::BaseRenderer::OnResize(uint32_t width, uint32_t height)
{
	assert(mDevice.Get());
	assert(mDisplay->GetSwapChain());
	assert(mInitCommandAllocator.Get());

	if (mClientWidth == width && mClientHeight == height)
	{
		return;
	}

	mClientWidth = width;
	mClientHeight = height;

	mTimer->Start();
	FlushCommandQueue();

	mCamera->SetLens(0.25f * DirectX::XM_PI, AspectRatio(), 1.0f, 1000.0f);

	ThrowIfFailed(mCommandList->Reset(mInitCommandAllocator.Get(), nullptr));

	mDisplay->OnResize();

	mCommandList->Close();
	vector<ID3D12CommandList*> cmdLists = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(1, cmdLists.data());
	FlushCommandQueue();

	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(mClientWidth);
	mScreenViewport.Height = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, (int)mClientWidth, (int)mClientHeight };
}

void Carol::BaseRenderer::SetPaused(bool state)
{
	mPaused = state;
}

bool Carol::BaseRenderer::Paused()
{
	return mPaused;
}

void Carol::BaseRenderer::SetMaximized(bool state)
{
	mMaximized = state;
}

bool Carol::BaseRenderer::Maximized()
{
	return mMaximized;
}

void Carol::BaseRenderer::SetMinimized(bool state)
{
	mMinimized = state;
}

bool Carol::BaseRenderer::Minimized()
{
	return mMinimized;
}

void Carol::BaseRenderer::SetResizing(bool state)
{
	mResizing = state;
}

bool Carol::BaseRenderer::Resizing()
{
	return mResizing;
}

void Carol::BaseRenderer::FlushCommandQueue()
{
	++mCpuFence;
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCpuFence));

	if (mFence->GetCompletedValue() < mCpuFence)
	{
		auto eventHandle = CreateEventEx(nullptr, LPCWSTR(nullptr), 0, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCpuFence, eventHandle));

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}
