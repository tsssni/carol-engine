#include <base_renderer.h>
#include <render_pass.h>
#include <dx12.h>
#include <scene.h>
#include <utils.h>

namespace Carol
{
	using std::unique_ptr;
	using std::unordered_map;
	using std::wstring;
	using std::vector;
	using std::make_unique;
	using std::to_wstring;
	using Microsoft::WRL::ComPtr;
	using namespace DirectX;

	unordered_map<wstring, unique_ptr<Shader>> gShaders;
	unordered_map<wstring, unique_ptr<PSO>> gPSOs;
}

float Carol::BaseRenderer::AspectRatio()
{
	return 1.0f * mClientWidth / mClientHeight;
}

void Carol::BaseRenderer::InitTimer()
{
	mTimer = make_unique<Timer>();
	mTimer->Reset();
}

void Carol::BaseRenderer::InitCamera()
{
	mCamera = make_unique<PerspectiveCamera>();
	mCamera->LookAt(XMFLOAT3(0.f, 10.f, -20.f), XMFLOAT3(0.f, 10.f, 0.f), XMFLOAT3(0.f, 1.f, 0.f));
	mCamera->UpdateViewMatrix();
}

Carol::BaseRenderer::BaseRenderer(HWND hWnd)
	:mhWnd(hWnd)
{
	InitTimer();
	InitCamera();

#if defined _DEBUG
	InitDebug();
#endif
	InitDxgiFactory();
	InitDevice();
	InitFence();

	InitCommandQueue();
	InitCommandAllocatorPool();
	InitCommandList();

	InitHeapManager();
	InitDescriptorManager();
	InitTextureManager();

	InitRenderPass();
	InitDisplay();
}

Carol::BaseRenderer::~BaseRenderer()
{
	FlushCommandQueue();
	StructuredBuffer::DeleteCounterResetBuffer();
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

void Carol::BaseRenderer::InitCommandAllocatorPool()
{
	mCommandAllocatorPool = make_unique<CommandAllocatorPool>(D3D12_COMMAND_LIST_TYPE_DIRECT, mDevice.Get());
}

void Carol::BaseRenderer::InitCommandList()
{
	mCommandAllocator = mCommandAllocatorPool->RequestAllocator(mGpuFenceValue);
	ComPtr<ID3D12GraphicsCommandList6> cmdList;
	ThrowIfFailed(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocator.Get(), nullptr, IID_PPV_ARGS(cmdList.GetAddressOf())));

	mCommandList = cmdList;
}

void Carol::BaseRenderer::InitRenderPass()
{
	RenderPass::Init(mDevice.Get());
}

void Carol::BaseRenderer::InitHeapManager()
{
	mHeapManager = make_unique<HeapManager>(mDevice.Get(), 1 << 29);
	StructuredBuffer::InitCounterResetBuffer(mHeapManager->GetUploadBuffersHeap(), mDevice.Get());
}

void Carol::BaseRenderer::InitDescriptorManager()
{
	mDescriptorManager = make_unique<DescriptorManager>(mDevice.Get());
}

void Carol::BaseRenderer::InitTextureManager()
{
	mTextureManager = make_unique<TextureManager>();
}

void Carol::BaseRenderer::InitDisplay()
{
	mDisplayPass = make_unique<DisplayPass>(mhWnd, mDxgiFactory.Get(), mDevice.Get(), mCommandQueue.Get(), 2);
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

	mTimer->Start();
	dynamic_cast<PerspectiveCamera*>(mCamera.get())->SetLens(0.25f * DirectX::XM_PI, AspectRatio(), 1.0f, 1000.0f);
	mDisplayPass->OnResize(mClientWidth, mClientHeight, mDevice.Get(), mHeapManager->GetDefaultBuffersHeap(), mDescriptorManager.get());
}

void Carol::BaseRenderer::SetPaused(bool state)
{
	mPaused = state;
}

bool Carol::BaseRenderer::IsPaused()
{
	return mPaused;
}

void Carol::BaseRenderer::SetMaximized(bool state)
{
	mMaximized = state;
}

bool Carol::BaseRenderer::IsZoomed()
{
	return mMaximized;
}

void Carol::BaseRenderer::SetMinimized(bool state)
{
	mMinimized = state;
}

bool Carol::BaseRenderer::IsIconic()
{
	return mMinimized;
}

void Carol::BaseRenderer::SetResizing(bool state)
{
	mResizing = state;
}

bool Carol::BaseRenderer::IsResizing()
{
	return mResizing;
}

void Carol::BaseRenderer::FlushCommandQueue()
{
	++mCpuFenceValue;
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCpuFenceValue));

	if (mFence->GetCompletedValue() < mCpuFenceValue)
	{
		auto eventHandle = CreateEventEx(nullptr, LPCWSTR(nullptr), 0, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCpuFenceValue, eventHandle));

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}
