#include "Display.h"
#include "DescriptorHeap.h"
#include "../Utils/Common.h"
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

void Carol::DisplayManager::InitDisplayManager(
	HWND hwnd,
	IDXGIFactory* factory,
	ID3D12CommandQueue* cmdQueue,
	uint32_t width,
	uint32_t height,
	uint32_t bufferCount,
	DXGI_FORMAT backBufferFormat,
	DXGI_FORMAT depthStencilFormat)
{
	mBackBufferFormat = backBufferFormat;
	mDepthStencilFormat = depthStencilFormat;

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferDesc.Width = width;
	swapChainDesc.BufferDesc.Height = height;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
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
	ThrowIfFailed(factory->CreateSwapChain(cmdQueue, &swapChainDesc, mSwapChain.GetAddressOf()));

	for (int i=0;i<bufferCount;++i)
	{
		mBackBuffer.push_back(make_unique<Resource>());
	}
	mDepthStencilBuffer = make_unique<DefaultBuffer>();
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

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DisplayManager::GetCurrBackBufferView(RtvDescriptorHeap* rtvHeap)
{
	return rtvHeap->GetCpuDescriptor(mCurrBackBufferIndex);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DisplayManager::GetDepthStencilView(DsvDescriptorHeap* dsvHeap)
{
	return dsvHeap->GetCpuDescriptor(0);
}

DXGI_FORMAT Carol::DisplayManager::GetBackBufferFormat()
{
	return mBackBufferFormat;
}

DXGI_FORMAT Carol::DisplayManager::GetDepthStencilFormat()
{
	return mDepthStencilFormat;
}

void Carol::DisplayManager::OnResize(
	ID3D12GraphicsCommandList* cmdList,
	RtvDescriptorHeap* rtvHeap,
	DsvDescriptorHeap* dsvHeap,
	uint32_t width,
	uint32_t height)
{
	ComPtr<ID3D12Device> device;
	cmdList->GetDevice(IID_PPV_ARGS(device.GetAddressOf()));

	for (auto& backBuffer : mBackBuffer)
	{
		backBuffer->Reset();
	}
	mDepthStencilBuffer->Reset();

	ThrowIfFailed(mSwapChain->ResizeBuffers(
		mBackBuffer.size(),
		width,
		height,
		mBackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	));

	mCurrBackBufferIndex = 0;

	for (int i = 0; i < mBackBuffer.size(); ++i)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(mBackBuffer[i]->GetAddressOf())));
		device->CreateRenderTargetView(mBackBuffer[i]->Get(), nullptr, rtvHeap->GetCpuDescriptor(i));
	}

	D3D12_RESOURCE_DESC depthStencilDesc;
	ZeroMemory(&depthStencilDesc, sizeof(D3D12_RESOURCE_DESC));
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = width;
	depthStencilDesc.Height = height;
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
	mDepthStencilBuffer->InitDefaultBuffer(device.Get(), D3D12_HEAP_FLAG_NONE, &depthStencilDesc, &optClear);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = mDepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	device->CreateDepthStencilView(mDepthStencilBuffer->Get(), &dsvDesc,dsvHeap->GetCpuDescriptor(0));
	mDepthStencilBuffer->TransitionBarrier(cmdList, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void Carol::DisplayManager::ClearAndSetRenderTargetAndDepthStencil(ID3D12GraphicsCommandList* cmdList, RtvDescriptorHeap* rtvHeap, DsvDescriptorHeap* dsvHeap, DirectX::XMVECTORF32 initColor, float initDepth, uint32_t initStencil)
{
	cmdList->ClearRenderTargetView(GetCurrBackBufferView(rtvHeap), initColor, 0, nullptr);
	cmdList->ClearDepthStencilView(GetDepthStencilView(dsvHeap), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, initDepth, initStencil, 0, nullptr);
	cmdList->OMSetRenderTargets(1, GetRvaluePtr(GetCurrBackBufferView(rtvHeap)), true, GetRvaluePtr(GetDepthStencilView(dsvHeap)));
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