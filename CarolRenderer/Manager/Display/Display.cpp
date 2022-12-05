#include "Display.h"
#include "../../DirectX/DescriptorAllocator.h"
#include "../../DirectX/Heap.h"
#include "../../Utils/Common.h"
#include <stdlib.h>
using std::make_unique;


IDXGISwapChain* Carol::DisplayManager::GetSwapChain()
{
	return mSwapChain.Get();
}

IDXGISwapChain** Carol::DisplayManager::GetAddressOfSwapChain()
{
	return mSwapChain.GetAddressOf();
}

uint32_t Carol::DisplayManager::GetBackBufferCount()
{
	return mBackBuffer.size();
}

Carol::DisplayManager::DisplayManager(
	RenderData* renderData,
	HWND hwnd,
	IDXGIFactory* factory,
	uint32_t width,
	uint32_t height,
	uint32_t bufferCount,
	DXGI_FORMAT backBufferFormat,
	DXGI_FORMAT depthStencilFormat)
	:Manager(renderData)
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
	ThrowIfFailed(factory->CreateSwapChain(renderData->CommandQueue, &swapChainDesc, mSwapChain.GetAddressOf()));
}

void Carol::DisplayManager::SetBackBufferIndex()
{
	mCurrBackBufferIndex = (mCurrBackBufferIndex + 1) % mBackBuffer.size();
}

Carol::Resource* Carol::DisplayManager::GetCurrBackBuffer()
{
	return mBackBuffer[mCurrBackBufferIndex].get();
}

Carol::Resource* Carol::DisplayManager::GetDepthStencilBuffer()
{
	return mDepthStencilBuffer.get();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DisplayManager::GetCurrBackBufferView()
{
	return GetRtv(mCurrBackBufferIndex);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DisplayManager::GetDepthStencilView()
{
	return mRenderData->DsvAllocator->GetCpuHandle(mDsvAllocInfo.get());
}

DXGI_FORMAT Carol::DisplayManager::GetBackBufferFormat()
{
	return mBackBufferFormat;
}

DXGI_FORMAT Carol::DisplayManager::GetDepthStencilFormat()
{
	return mDepthStencilFormat;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::DisplayManager::GetDepthStencilSrv()
{
	return GetShaderGpuSrv(DEPTH_STENCIL_SRV);
}

void Carol::DisplayManager::Draw()
{
}

void Carol::DisplayManager::Update()
{
}

void Carol::DisplayManager::OnResize()
{
	static uint32_t width = 0;
    static uint32_t height = 0;

    if (width != *mRenderData->ClientWidth || height != *mRenderData->ClientHeight)
    {
		Manager::OnResize();

        width = *mRenderData->ClientWidth;
        height = *mRenderData->ClientHeight;

        InitResources();
    }
}

void Carol::DisplayManager::ReleaseIntermediateBuffers()
{
}

void Carol::DisplayManager::Present()
{
	ComPtr<ID3D12Device> device;
	mDepthStencilBuffer->Get()->GetDevice(IID_PPV_ARGS(device.GetAddressOf()));

	if (FAILED(mSwapChain->Present(0, 0)))
	{
		ThrowIfFailed(device->GetDeviceRemovedReason());
	}
	SetBackBufferIndex();
}

void Carol::DisplayManager::InitRootSignature()
{
}

void Carol::DisplayManager::InitShaders()
{
}

void Carol::DisplayManager::InitPSOs()
{
}

void Carol::DisplayManager::InitResources()
{
	for (int i = 0; i < mBackBuffer.size(); ++i)
	{
		mBackBuffer[i] = make_unique<Resource>();
	}

	ThrowIfFailed(mSwapChain->ResizeBuffers(
		mBackBuffer.size(),
		*mRenderData->ClientWidth,
		*mRenderData->ClientHeight,
		mBackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	));

	mCurrBackBufferIndex = 0;

	D3D12_RESOURCE_DESC depthStencilDesc = {};
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = *mRenderData->ClientWidth;
	depthStencilDesc.Height = *mRenderData->ClientHeight;
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
	
	mDepthStencilBuffer = make_unique<DefaultResource>(&depthStencilDesc, mRenderData->RtvDsvTexturesHeap, D3D12_RESOURCE_STATE_DEPTH_WRITE, &optClear);

	InitDescriptors();
}

void Carol::DisplayManager::InitDescriptors()
{
	mRenderData->CbvSrvUavAllocator->CpuAllocate(1, mCpuCbvSrvUavAllocInfo.get());
	mRenderData->RtvAllocator->CpuAllocate(mBackBuffer.size(), mRtvAllocInfo.get());
	mRenderData->DsvAllocator->CpuAllocate(DISPLAY_SRV_COUNT, mDsvAllocInfo.get());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	auto depthStencilCpuSrv = GetCpuSrv(DEPTH_STENCIL_SRV);
	mRenderData->Device->CreateShaderResourceView(mDepthStencilBuffer->Get(), &srvDesc, depthStencilCpuSrv);

	for (int i = 0; i < mBackBuffer.size(); ++i)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(mBackBuffer[i]->GetAddressOf())));
		mRenderData->Device->CreateRenderTargetView(mBackBuffer[i]->Get(), nullptr, GetRtv(i));
	}

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = mDepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;

	mRenderData->Device->CreateDepthStencilView(mDepthStencilBuffer->Get(), &dsvDesc, GetDsv(0));
}
