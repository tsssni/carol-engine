#include "BaseRenderer.h"
#include "DirectX/Display.h"
#include "Utils/Common.h"
#include <DirectXMath.h>
#include <memory>

using std::make_unique;

float Carol::BaseRenderer::AspectRatio()
{
	return 1.0f * mClientWidth / mClientHeight;
}

void Carol::BaseRenderer::InitRtvDsvDescriptorHeaps()
{
	mRtvHeap = make_unique<RtvDescriptorHeap>();
	mDsvHeap = make_unique<DsvDescriptorHeap>();

	mRtvHeap->InitRtvDescriptorHeap(mDevice.Get(), 2);
	mDsvHeap->InitDsvDescriptorHeap(mDevice.Get(), 1);
}

void Carol::BaseRenderer::InitRenderer(HWND hWnd, uint32_t width, uint32_t height)
{
	mDisplayManager = make_unique<DisplayManager>();
	
	mTimer = make_unique<GameTimer>();
	mTimer->Reset();
	mCamera = make_unique<Camera>();

	mhWnd = hWnd;

#if defined(DEBUG) || defined(_DEBUG)
	InitDebug();
#endif
	InitDxgiFactory();
	InitDevice();
	InitFence();

	InitCommandAllocator();
	InitCommandList();
	InitCommandQueue();

	mDisplayManager->InitDisplayManager(hWnd, mDxgiFactory.Get(), mCommandQueue.Get(), width, height, 2);
	InitRtvDsvDescriptorHeaps();

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
}

void Carol::BaseRenderer::InitCommandAllocator()
{
	ThrowIfFailed(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mInitCommandAllocator.GetAddressOf())));
}

void Carol::BaseRenderer::InitCommandList()
{
	ThrowIfFailed(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mInitCommandAllocator.Get(), nullptr, IID_PPV_ARGS(mCommandList.GetAddressOf())));
	mCommandList->Close();
}

void Carol::SkinnedModelInfo::UpdateSkinnedModel(float dt)
{
	TimePos += dt;
	if (TimePos > SkinnedInfo->GetClipEndTime(ClipName))
	{
		TimePos = 0.0f;
	}

	SkinnedInfo->GetFinalTransforms(ClipName, TimePos, FinalTransforms);
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
	assert(mDisplayManager->GetSwapChain());
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

	mDisplayManager->OnResize(
		mCommandList.Get(),
		mRtvHeap.get(),
		mDsvHeap.get(),
		mClientWidth,
		mClientHeight);

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
	++mCurrFence;
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrFence));

	if (mFence->GetCompletedValue() < mCurrFence)
	{
		auto eventHandle = CreateEventEx(nullptr, LPCWSTR(nullptr), 0, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFence, eventHandle));

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}
