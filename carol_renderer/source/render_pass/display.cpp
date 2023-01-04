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
	DXGI_FORMAT backBufferFormat)
	:RenderPass(globalResources), mBackBufferFormat(backBufferFormat),mBackBuffer(bufferCount)
{
	mBackBufferFormat = backBufferFormat;
	mBackBuffer.resize(bufferCount);

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

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Display::GetCurrBackBufferRtv()
{
	return GetRtv(mCurrBackBufferIndex);
}

DXGI_FORMAT Carol::Display::GetBackBufferFormat()
{
	return mBackBufferFormat;
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
		DeallocateDescriptors();

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
	if (FAILED(mSwapChain->Present(0, 0)))
	{
		ThrowIfFailed(mGlobalResources->Device->GetDeviceRemovedReason());
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

	InitDescriptors();
}

void Carol::Display::InitDescriptors()
{
	mGlobalResources->RtvAllocator->CpuAllocate(mBackBuffer.size(), mRtvAllocInfo.get());
	
	for (int i = 0; i < mBackBuffer.size(); ++i)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(mBackBuffer[i]->GetAddressOf())));
		mGlobalResources->Device->CreateRenderTargetView(mBackBuffer[i]->Get(), nullptr, GetRtv(i));
	}
}
