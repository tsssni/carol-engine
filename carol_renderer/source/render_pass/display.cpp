#include <render_pass/display.h>
#include <render_pass/global_resources.h>
#include <dx12/resource.h>
#include <dx12/descriptor_allocator.h>
#include <dx12/root_signature.h>
#include <dx12/heap.h>
#include <utils/Common.h>
#include <stdlib.h>

namespace Carol {
	using std::make_unique;
	using Microsoft::WRL::ComPtr;
}


IDXGISwapChain* Carol::Display::GetSwapChain()
{
	return mSwapChain.Get();
}

IDXGISwapChain** Carol::Display::GetAddressOfSwapChain()
{
	return mSwapChain.GetAddressOf();
}

uint32_t Carol::Display::GetBackBufferCount()
{
	return mBackBuffer.size();
}

Carol::Display::Display(
	GlobalResources* globalResources,
	HWND hwnd,
	IDXGIFactory* factory,
	uint32_t width,
	uint32_t height,
	uint32_t bufferCount,
	DXGI_FORMAT backBufferFormat,
	DXGI_FORMAT depthStencilFormat)
	:RenderPass(globalResources)
{
	mBackBufferFormat = backBufferFormat;
	mBackBuffer.resize(bufferCount);
	mDepthStencilFormat = depthStencilFormat;

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferDesc.Width = width;
	swapChainDesc.BufferDesc.Height = height;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 144;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferDesc.Format = backBufferFormat;
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
	ThrowIfFailed(factory->CreateSwapChain(globalResources->CommandQueue, &swapChainDesc, mSwapChain.GetAddressOf()));
}

void Carol::Display::SetBackBufferIndex()
{
	mCurrBackBufferIndex = (mCurrBackBufferIndex + 1) % mBackBuffer.size();
}

Carol::Resource* Carol::Display::GetCurrBackBuffer()
{
	return mBackBuffer[mCurrBackBufferIndex].get();
}

Carol::Resource* Carol::Display::GetDepthStencilBuffer()
{
	return mDepthStencilBuffer.get();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Display::GetCurrBackBufferRtv()
{
	return GetRtv(mCurrBackBufferIndex);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Display::GetDepthStencilDsv()
{
	return mGlobalResources->DsvAllocator->GetCpuHandle(mDsvAllocInfo.get());
}

DXGI_FORMAT Carol::Display::GetBackBufferFormat()
{
	return mBackBufferFormat;
}

DXGI_FORMAT Carol::Display::GetDepthStencilFormat()
{
	return mDepthStencilFormat;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::Display::GetDepthStencilSrv()
{
	return GetShaderGpuSrv(DEPTH_STENCIL_SRV);
}

void Carol::Display::Draw()
{
}

void Carol::Display::Update()
{
}

void Carol::Display::OnResize()
{
	static uint32_t width = 0;
    static uint32_t height = 0;

    if (width != *mGlobalResources->ClientWidth || height != *mGlobalResources->ClientHeight)
    {
		RenderPass::OnResize();

        width = *mGlobalResources->ClientWidth;
        height = *mGlobalResources->ClientHeight;

        InitResources();
    }
}

void Carol::Display::ReleaseIntermediateBuffers()
{
}

void Carol::Display::Present()
{
	ComPtr<ID3D12Device> device;
	mDepthStencilBuffer->Get()->GetDevice(IID_PPV_ARGS(device.GetAddressOf()));

	if (FAILED(mSwapChain->Present(0, 0)))
	{
		ThrowIfFailed(device->GetDeviceRemovedReason());
	}
	SetBackBufferIndex();
}

void Carol::Display::InitShaders()
{
}

void Carol::Display::InitPSOs()
{
}

void Carol::Display::InitResources()
{
	for (int i = 0; i < mBackBuffer.size(); ++i)
	{
		mBackBuffer[i] = make_unique<Resource>();
	}

	ThrowIfFailed(mSwapChain->ResizeBuffers(
		mBackBuffer.size(),
		*mGlobalResources->ClientWidth,
		*mGlobalResources->ClientHeight,
		mBackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	));

	mCurrBackBufferIndex = 0;

	D3D12_RESOURCE_DESC depthStencilDesc = {};
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = *mGlobalResources->ClientWidth;
	depthStencilDesc.Height = *mGlobalResources->ClientHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = mDepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	
	mDepthStencilBuffer = make_unique<DefaultResource>(&depthStencilDesc, mGlobalResources->TexturesHeap, D3D12_RESOURCE_STATE_DEPTH_WRITE, &optClear);

	InitDescriptors();
}

void Carol::Display::InitDescriptors()
{
	mGlobalResources->CbvSrvUavAllocator->CpuAllocate(DISPLAY_SRV_COUNT, mCpuSrvAllocInfo.get());
	mGlobalResources->RtvAllocator->CpuAllocate(mBackBuffer.size(), mRtvAllocInfo.get());
	mGlobalResources->DsvAllocator->CpuAllocate(DISPLAY_DSV_COUNT, mDsvAllocInfo.get());
	
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	auto depthStencilCpuSrv = GetCpuSrv(DEPTH_STENCIL_SRV);
	mGlobalResources->Device->CreateShaderResourceView(mDepthStencilBuffer->Get(), &srvDesc, depthStencilCpuSrv);
	
	for (int i = 0; i < mBackBuffer.size(); ++i)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(mBackBuffer[i]->GetAddressOf())));
		mGlobalResources->Device->CreateRenderTargetView(mBackBuffer[i]->Get(), nullptr, GetRtv(i));
	}

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = mDepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;

	mGlobalResources->Device->CreateDepthStencilView(mDepthStencilBuffer->Get(), &dsvDesc, GetDsv(DEPTH_STENCIL_DSV));
}
