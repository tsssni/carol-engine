#include <render_pass/display_pass.h>
#include <global.h>
#include <dx12.h>
#include <utils/common.h>
#include <stdlib.h>

namespace Carol {
	using std::make_unique;
	using Microsoft::WRL::ComPtr;
}

Carol::DisplayPass::~DisplayPass()
{
	if (mBackBufferRtvAllocInfo && mBackBufferRtvAllocInfo->Manager)
	{
		mBackBufferRtvAllocInfo->Manager->RtvDeallocate(mBackBufferRtvAllocInfo.release());
	}
}

IDXGISwapChain* Carol::DisplayPass::GetSwapChain()
{
	return mSwapChain.Get();
}

IDXGISwapChain** Carol::DisplayPass::GetAddressOfSwapChain()
{
	return mSwapChain.GetAddressOf();
}

uint32_t Carol::DisplayPass::GetBackBufferCount()
{
	return mBackBuffer.size();
}

Carol::DisplayPass::DisplayPass(
	HWND hwnd,
	IDXGIFactory* factory,
	ID3D12CommandQueue* cmdQueue,
	uint32_t bufferCount,
	DXGI_FORMAT backBufferFormat)
	:mBackBuffer(bufferCount),
	mBackBufferFormat(backBufferFormat),
	mBackBufferRtvAllocInfo(make_unique<DescriptorAllocInfo>())
{
	mBackBufferFormat = backBufferFormat;
	mBackBuffer.resize(bufferCount);

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferDesc.Width = 0;
	swapChainDesc.BufferDesc.Height = 0;
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
	ThrowIfFailed(factory->CreateSwapChain(cmdQueue, &swapChainDesc, mSwapChain.GetAddressOf()));
}

void Carol::DisplayPass::SetBackBufferIndex()
{
	mCurrBackBufferIndex = (mCurrBackBufferIndex + 1) % mBackBuffer.size();
}

Carol::Resource* Carol::DisplayPass::GetCurrBackBuffer()
{
	return mBackBuffer[mCurrBackBufferIndex].get();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DisplayPass::GetCurrBackBufferRtv()
{
	return mBackBufferRtvAllocInfo->Manager->GetRtvHandle(mBackBufferRtvAllocInfo.get(), mCurrBackBufferIndex);
}

DXGI_FORMAT Carol::DisplayPass::GetBackBufferFormat()
{
	return mBackBufferFormat;
}

void Carol::DisplayPass::Draw(ID3D12GraphicsCommandList* cmdList)
{
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

	mCurrBackBufferIndex = 0;
	InitDescriptors(descriptorManager);
}

void Carol::DisplayPass::InitDescriptors(DescriptorManager* descriptorManager)
{
	ComPtr<ID3D12Device> device;
	mSwapChain->GetDevice(IID_PPV_ARGS(device.GetAddressOf()));

	if (!mBackBufferRtvAllocInfo->NumDescriptors)
	{
		mBackBufferRtvAllocInfo = descriptorManager->RtvAllocate(mBackBuffer.size());
	}
	
	for (int i = 0; i < mBackBuffer.size(); ++i)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(mBackBuffer[i]->GetAddressOf())));
		device->CreateRenderTargetView(mBackBuffer[i]->Get(), nullptr, descriptorManager->GetRtvHandle(mBackBufferRtvAllocInfo.get(), i));
	}
}
