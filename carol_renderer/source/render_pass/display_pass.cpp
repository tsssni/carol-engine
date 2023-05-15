#include <render_pass/display_pass.h>
#include <dx12/descriptor.h>
#include <dx12/heap.h>
#include <dx12/resource.h>
#include <dx12/pipeline_state.h>
#include <dx12/shader.h>
#include <global.h>
#include <stdlib.h>
#include <vector>
#include <string_view>

namespace Carol {
	using std::make_unique;
	using std::vector;
	using std::wstring_view;
	using Microsoft::WRL::ComPtr;
}

IDXGISwapChain* Carol::DisplayPass::GetSwapChain()
{
	return mSwapChain.Get();
}

IDXGISwapChain** Carol::DisplayPass::GetAddressOfSwapChain()
{
	return mSwapChain.GetAddressOf();
}

uint32_t Carol::DisplayPass::GetBackBufferCount()const
{
	return mBackBuffer.size();
}

Carol::DisplayPass::DisplayPass(
	HWND hwnd,
	uint32_t bufferCount,
	DXGI_FORMAT backBufferFormat,
	DXGI_FORMAT frameFormat,
	DXGI_FORMAT depthStencilFormat)
	:mBackBuffer(bufferCount),
	mBackBufferFormat(backBufferFormat),
	mFrameFormat(frameFormat),
	mDepthStencilFormat(depthStencilFormat),
	mBackBufferRtvAllocInfo(make_unique<DescriptorAllocInfo>())
{
	InitPSOs();
	InitSwapChain(hwnd);
}

Carol::DisplayPass::~DisplayPass()
{
	if (mBackBufferRtvAllocInfo && mBackBufferRtvAllocInfo->Manager)
	{
		mBackBufferRtvAllocInfo->Manager->RtvDeallocate(mBackBufferRtvAllocInfo.release());
	}
}

void Carol::DisplayPass::SetBackBufferIndex()
{
	mBackBufferIdx = (mBackBufferIdx + 1) % mBackBuffer.size();
}

Carol::ColorBuffer* Carol::DisplayPass::GetFrameMap()const
{
	return mFrameMap.get();
}

Carol::ColorBuffer* Carol::DisplayPass::GetDepthStencilMap()const
{
	return mDepthStencilMap.get();
}

uint32_t Carol::DisplayPass::GetFrameMapUavIdx()const
{
	return mFrameMap->GetGpuUavIdx();
}

uint32_t Carol::DisplayPass::GetHistMapUavIdx() const
{
	return mHistMap->GetGpuUavIdx();
}

uint32_t Carol::DisplayPass::GetDepthStencilSrvIdx()const
{
	return mDepthStencilMap->GetGpuSrvIdx();
}

void Carol::DisplayPass::Draw()
{
	auto backBufferRtv = mBackBufferRtvAllocInfo->Manager->GetRtvHandle(mBackBufferRtvAllocInfo.get(), mBackBufferIdx);
	mFrameMap->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	gGraphicsCommandList->OMSetRenderTargets(1, &backBufferRtv, true, nullptr);

	gGraphicsCommandList->SetPipelineState(mDisplayMeshPSO->Get());
	static_cast<ID3D12GraphicsCommandList6*>(gGraphicsCommandList.Get())->DispatchMesh(1, 1, 1);

	mBackBuffer[mBackBufferIdx]->Transition( D3D12_RESOURCE_STATE_PRESENT);
	mFrameMap->Transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void Carol::DisplayPass::Present()
{
	ComPtr<ID3D12Device> device;
	mSwapChain->GetDevice(IID_PPV_ARGS(device.GetAddressOf()));

	if (FAILED(mSwapChain->Present(0, 0)))
	{
		ThrowIfFailed(device->GetDeviceRemovedReason());
	}

	SetBackBufferIndex();
}

void Carol::DisplayPass::InitPSOs()
{
	mDisplayMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mDisplayMeshPSO->SetRenderTargetFormat(mBackBufferFormat);
	mDisplayMeshPSO->SetMS(gShaderManager->LoadShader("shader/dxil/screen_ms.dxil"));
	mDisplayMeshPSO->SetPS(gShaderManager->LoadShader("shader/dxil/display_ps.dxil"));
	mDisplayMeshPSO->Finalize();
}

void Carol::DisplayPass::InitBuffers()
{
	float frameColor[4] = { 0.f,0.f,0.f,1.f };
	D3D12_CLEAR_VALUE frameOptClearValue = CD3DX12_CLEAR_VALUE(mFrameFormat, frameColor);
	mFrameMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mFrameFormat,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		&frameOptClearValue);
	mHistMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mFrameFormat,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	D3D12_CLEAR_VALUE depthStencilOptClearValue = CD3DX12_CLEAR_VALUE(GetDsvFormat(mDepthStencilFormat), 1.f, 0);
	mDepthStencilMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mDepthStencilFormat,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		&depthStencilOptClearValue);

	for (int i = 0; i < mBackBuffer.size(); ++i)
	{
		mBackBuffer[i] = make_unique<Resource>();
	}

	ThrowIfFailed(mSwapChain->ResizeBuffers(
		mBackBuffer.size(),
		mWidth,
		mHeight,
		mBackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	));

	if (!mBackBufferRtvAllocInfo->NumDescriptors)
	{
		mBackBufferRtvAllocInfo = gDescriptorManager->RtvAllocate(mBackBuffer.size());
	}

	for (int i = 0; i < mBackBuffer.size(); ++i)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(mBackBuffer[i]->GetAddressOf())));
		gDevice->CreateRenderTargetView(mBackBuffer[i]->Get(), nullptr, gDescriptorManager->GetRtvHandle(mBackBufferRtvAllocInfo.get(), i));
	}
	
	mBackBufferIdx = 0;
}

void Carol::DisplayPass::InitSwapChain(HWND hwnd)
{
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferDesc.Width = 0;
	swapChainDesc.BufferDesc.Height = 0;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 144;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferDesc.Format = mBackBufferFormat;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapChainDesc.BufferCount = mBackBuffer.size();
	swapChainDesc.OutputWindow = hwnd;
	swapChainDesc.Windowed = true;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	mSwapChain.Reset();
	ThrowIfFailed(gDxgiFactory->CreateSwapChain(gCommandQueue.Get(), &swapChainDesc, mSwapChain.GetAddressOf()));
}
