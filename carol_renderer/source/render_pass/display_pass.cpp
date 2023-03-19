#include <render_pass/display_pass.h>
#include <global.h>
#include <dx12.h>
#include <utils/common.h>
#include <stdlib.h>

namespace Carol {
	using std::make_unique;
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
	IDXGIFactory* factory,
	ID3D12CommandQueue* cmdQueue,
	uint32_t bufferCount,
	DXGI_FORMAT frameFormat,
	DXGI_FORMAT depthStencilFormat)
	:mBackBuffer(bufferCount),
	mFrameFormat(frameFormat),
	mDepthStencilFormat(depthStencilFormat)
{
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferDesc.Width = 0;
	swapChainDesc.BufferDesc.Height = 0;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 144;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferDesc.Format = frameFormat;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapChainDesc.BufferCount = bufferCount;
	swapChainDesc.OutputWindow = hwnd;
	swapChainDesc.Windowed = true;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	mSwapChain.Reset();
	ThrowIfFailed(factory->CreateSwapChain(cmdQueue, &swapChainDesc, mSwapChain.GetAddressOf()));
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

DXGI_FORMAT Carol::DisplayPass::GetFrameFormat()const
{
	return mFrameFormat;
}

uint32_t Carol::DisplayPass::GetFrameMapUavIdx()const
{
	return mFrameMap->GetGpuUavIdx();
}

uint32_t Carol::DisplayPass::GetDepthStencilSrvIdx()const
{
	return mDepthStencilMap->GetGpuSrvIdx();
}

DXGI_FORMAT Carol::DisplayPass::GetDepthStencilFormat() const
{
	return mDepthStencilFormat;
}

void Carol::DisplayPass::Draw(ID3D12GraphicsCommandList* cmdList)
{
	mFrameMap->Transition(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	mBackBuffer[mBackBufferIdx]->Transition(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);
	cmdList->CopyResource(mBackBuffer[mBackBufferIdx]->Get(), mFrameMap->Get());
	mBackBuffer[mBackBufferIdx]->Transition(cmdList, D3D12_RESOURCE_STATE_PRESENT);
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

void Carol::DisplayPass::InitShaders()
{
}

void Carol::DisplayPass::InitPSOs(ID3D12Device* device)
{
}

void Carol::DisplayPass::InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)
{
	D3D12_CLEAR_VALUE frameOptClearValue = CD3DX12_CLEAR_VALUE(mFrameFormat, DirectX::Colors::Gray);
	mFrameMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mFrameFormat,
		device,
		heap,
		descriptorManager,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		&frameOptClearValue);

	D3D12_CLEAR_VALUE depthStencilOptClearValue = CD3DX12_CLEAR_VALUE(GetDsvFormat(mDepthStencilFormat), 1.f, 0);
	mDepthStencilMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mDepthStencilFormat,
		device,
		heap,
		descriptorManager,
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
		mFrameFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	));

	for (int i = 0; i < mBackBuffer.size(); ++i)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(mBackBuffer[i]->GetAddressOf())));
	}
	
	mBackBufferIdx = 0;
}