#include <base_renderer.h>
#include <render_pass/display.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/descriptor.h>
#include <dx12/pipeline_state.h>
#include <dx12/root_signature.h>
#include <dx12/shader.h>
#include <dx12/indirect_command.h>
#include <scene/timer.h>
#include <scene/camera.h>
#include <scene/skinned_data.h>
#include <utils/bitset.h>
#include <utils/buddy.h>
#include <utils/common.h>
#include <DirectXMath.h>
#include <memory>

namespace Carol
{
	using std::unique_ptr;
	using std::unordered_map;
	using std::vector;
	using std::make_unique;
	using std::to_wstring;
	using Microsoft::WRL::ComPtr;

	ComPtr<ID3D12Device> gDevice;
	ComPtr<ID3D12CommandQueue> gCommandQueue;
	ComPtr<ID3D12GraphicsCommandList> gCommandList;

	unique_ptr<RootSignature> gRootSignature;
	ComPtr<ID3D12CommandSignature> gCommandSignature;
	
	unique_ptr<HeapManager> gHeapManager;
	unique_ptr<DescriptorManager> gDescriptorManager;

	unordered_map<wstring, unique_ptr<Shader>> gShaders;
	unordered_map<wstring, unique_ptr<PSO>> gPSOs;
	std::unique_ptr<DisplayPass> gDisplayPass;

	uint32_t gNumFrame;
	uint32_t gCurrFrame;
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
	mCamera = make_unique<Camera>();
	mCamera->SetPosition(0.0f, 5.0f, -5.0f);
}

Carol::BaseRenderer::BaseRenderer(HWND hWnd, uint32_t width, uint32_t height)
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

	InitCommandAllocator();
	InitCommandList();
	InitCommandQueue();

	InitRootSignature();
	InitCommandSignature();

	InitHeapManager();
	InitDescriptorManager();	
	InitDisplay();
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
	
	gDevice = device;
}

void Carol::BaseRenderer::InitFence()
{
	ComPtr<ID3D12Fence> fence;
	ThrowIfFailed(gDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())));
	mFence = fence;
}

void Carol::BaseRenderer::InitCommandQueue()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	ThrowIfFailed(gDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(gCommandQueue.GetAddressOf())));
}

void Carol::BaseRenderer::InitCommandAllocator()
{
	ThrowIfFailed(gDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mInitCommandAllocator.GetAddressOf())));

	gNumFrame = 3;
	gCurrFrame = 0;

	mFrameAllocator.resize(gNumFrame);
	for (int i = 0; i < gNumFrame; ++i)
	{
		ThrowIfFailed(gDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mFrameAllocator[i].GetAddressOf())));
	}
}

void Carol::BaseRenderer::InitCommandList()
{
	ComPtr<ID3D12GraphicsCommandList6> cmdList;

	ThrowIfFailed(gDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mInitCommandAllocator.Get(), nullptr, IID_PPV_ARGS(cmdList.GetAddressOf())));
	cmdList->Close();

	gCommandList = cmdList;
}

void Carol::BaseRenderer::InitCommandSignature()
{
	D3D12_INDIRECT_ARGUMENT_DESC argDesc[3];

	argDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
	argDesc[0].ConstantBufferView.RootParameterIndex = MESH_CB;
	
	argDesc[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
	argDesc[1].ConstantBufferView.RootParameterIndex = SKINNED_CB;

	argDesc[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;
	
	D3D12_COMMAND_SIGNATURE_DESC cmdSigDesc;
	cmdSigDesc.pArgumentDescs = argDesc;
	cmdSigDesc.NumArgumentDescs = _countof(argDesc);
	cmdSigDesc.ByteStride = sizeof(IndirectCommand);
	cmdSigDesc.NodeMask = 0;

	ThrowIfFailed(gDevice->CreateCommandSignature(&cmdSigDesc, gRootSignature->Get(), IID_PPV_ARGS(gCommandSignature.GetAddressOf())));
}

void Carol::BaseRenderer::InitRootSignature()
{
	Shader::InitCompiler();
	gRootSignature = make_unique<RootSignature>();
}

void Carol::BaseRenderer::InitHeapManager()
{
	gHeapManager = make_unique<HeapManager>();
	StructuredBuffer::InitCounterResetBuffer(gHeapManager->GetUploadBuffersHeap());
}

void Carol::BaseRenderer::InitDescriptorManager()
{
	gDescriptorManager = make_unique<DescriptorManager>();
}

void Carol::BaseRenderer::InitDisplay()
{
	gDisplayPass = make_unique<DisplayPass>(mhWnd, mDxgiFactory.Get(), mClientWidth, mClientHeight, 2);
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
	mCamera->SetLens(0.25f * DirectX::XM_PI, AspectRatio(), 1.0f, 1000.0f);
	gDisplayPass->OnResize(mClientWidth, mClientHeight);
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
	ThrowIfFailed(gCommandQueue->Signal(mFence.Get(), mCpuFence));

	if (mFence->GetCompletedValue() < mCpuFence)
	{
		auto eventHandle = CreateEventEx(nullptr, LPCWSTR(nullptr), 0, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCpuFence, eventHandle));

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}
