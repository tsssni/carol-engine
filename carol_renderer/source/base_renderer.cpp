#include <base_renderer.h>
#include <render_pass/global_resources.h>
#include <render_pass/display.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/descriptor.h>
#include <dx12/root_signature.h>
#include <dx12/shader.h>
#include <scene/timer.h>
#include <scene/camera.h>
#include <scene/skinned_data.h>
#include <utils/bitset.h>
#include <utils/buddy.h>
#include <utils/common.h>
#include <DirectXMath.h>
#include <memory>

namespace Carol {
	using std::vector;
	using std::make_unique;
	using std::to_wstring;
	using Microsoft::WRL::ComPtr;
}

float Carol::BaseRenderer::AspectRatio()
{
	return 1.0f * mClientWidth / mClientHeight;
}

void Carol::BaseRenderer::InitTimer()
{
	mTimer = make_unique<Timer>();
	mTimer->Reset();
	mGlobalResources->Timer = mTimer.get();
}

void Carol::BaseRenderer::InitCamera()
{
	mCamera = make_unique<Camera>();
	mCamera->SetPosition(0.0f, 5.0f, -5.0f);
	mGlobalResources->Camera = mCamera.get();
}

Carol::BaseRenderer::BaseRenderer(HWND hWnd, uint32_t width, uint32_t height)
	:mhWnd(hWnd),mGlobalResources(make_unique<GlobalResources>())
{
	InitTimer();
	InitCamera();

#if defined _DEBUG
	InitDebug();
#endif
	InitDxgiFactory();
	InitDevice();
	InitFence();

	InitCommandAllocator();
	InitCommandList();
	InitCommandQueue();

	InitHeapManager();
	InitDescriptorManager();	
	InitRootSignature();
	InitDisplay();
	
	InitShaders();
	InitPSOs();
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
	ComPtr<ID3D12Device2> device;
	ThrowIfFailed(D3D12CreateDevice(device.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(device.GetAddressOf())));

	mGlobalResources->Device = device.Get();
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
	mGlobalResources->CommandQueue = mCommandQueue.Get();
}

void Carol::BaseRenderer::InitCommandAllocator()
{
	ThrowIfFailed(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mInitCommandAllocator.GetAddressOf())));

	mFrameAllocator.resize(mNumFrame);
	for (int i = 0; i < mNumFrame; ++i)
	{
		ThrowIfFailed(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mFrameAllocator[i].GetAddressOf())));
	}

	mGlobalResources->NumFrame = mNumFrame;
	mGlobalResources->CurrFrame = &mCurrFrame;
}

void Carol::BaseRenderer::InitCommandList()
{
	ComPtr<ID3D12GraphicsCommandList6> cmdList;

	ThrowIfFailed(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mInitCommandAllocator.Get(), nullptr, IID_PPV_ARGS(cmdList.GetAddressOf())));
	cmdList->Close();

	mGlobalResources->CommandList = cmdList.Get();
	mCommandList = cmdList;
}

void Carol::BaseRenderer::InitRootSignature()
{
	Shader::InitCompiler();
	mRootSignature = make_unique<RootSignature>(mDevice.Get());
	mGlobalResources->RootSignature = mRootSignature.get();
}

void Carol::BaseRenderer::InitHeapManager()
{
	mHeapManager = make_unique<HeapManager>(mDevice.Get());
	mGlobalResources->HeapManager = mHeapManager.get();
	StructuredBuffer::InitCounterResetBuffer(mHeapManager->GetUploadBuffersHeap());
}

void Carol::BaseRenderer::InitDescriptorManager()
{
	mDescriptorManager = make_unique<DescriptorManager>(mDevice.Get());
	mGlobalResources->DescriptorManager = mDescriptorManager.get();
}

void Carol::BaseRenderer::InitDisplay()
{
	mGlobalResources->ScreenViewport = &mScreenViewport;
	mGlobalResources->ScissorRect = &mScissorRect;
	
	mGlobalResources->ClientWidth = &mClientWidth;
	mGlobalResources->ClientHeight = &mClientHeight;

	mDisplay=make_unique<Display>(mGlobalResources.get(), mhWnd, mDxgiFactory.Get(), mClientWidth, mClientHeight, 2);
	mGlobalResources->Display = mDisplay.get();
}

void Carol::BaseRenderer::InitShaders()
{
	mGlobalResources->Shaders = &mShaders;
}

void Carol::BaseRenderer::InitPSOs()
{
	mGlobalResources->PSOs = &mPSOs;
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

void Carol::BaseRenderer::OnResize(uint32_t width, uint32_t height, bool init)
{
	if (mClientWidth == width && mClientHeight == height)
	{
		return;
	}

	mClientWidth = width;
	mClientHeight = height;

	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(mClientWidth);
	mScreenViewport.Height = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, (int)mClientWidth, (int)mClientHeight };

	mTimer->Start();
	mCamera->SetLens(0.25f * DirectX::XM_PI, AspectRatio(), 1.0f, 1000.0f);
	mDisplay->OnResize();
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
