#include <render_pass/display_pass.h>
#include <global.h>
#include <dx12.h>
#include <utils/common.h>
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
	IDXGIFactory* factory,
	ID3D12Device* device,
	ID3D12CommandQueue* cmdQueue,
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
	InitShaders();
	InitPSOs(device);
	InitSwapChain(hwnd, factory, cmdQueue);
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
	cmdList->OMSetRenderTargets(1, GetRvaluePtr(mBackBufferRtvAllocInfo->Manager->GetRtvHandle(mBackBufferRtvAllocInfo.get(), mBackBufferIdx)), true, nullptr);
	cmdList->SetPipelineState(gPSOs[L"Display"]->Get());
	static_cast<ID3D12GraphicsCommandList6*>(cmdList)->DispatchMesh(1, 1, 1);
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
	vector<wstring_view> nullDefines = {};

	if (gShaders.count(L"ScreenMS") == 0)
	{
		gShaders[L"ScreenMS"] = make_unique<Shader>(L"shader\\screen_ms.hlsl", nullDefines, L"main", L"ms_6_6");
	}

	if (gShaders.count(L"DisplayPS") == 0)
	{
		gShaders[L"DisplayPS"] = make_unique<Shader>(L"shader\\display_ps.hlsl", nullDefines, L"main", L"ps_6_6");
	}
}

void Carol::DisplayPass::InitPSOs(ID3D12Device* device)
{
	if (gPSOs.count(L"Display") == 0)
	{
		auto displayMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
		displayMeshPSO->SetRootSignature(sRootSignature.get());
		displayMeshPSO->SetRenderTargetFormat(mBackBufferFormat);
		displayMeshPSO->SetMS(gShaders[L"ScreenMS"].get());
		displayMeshPSO->SetPS(gShaders[L"DisplayPS"].get());
		displayMeshPSO->Finalize(device);

		gPSOs[L"Display"] = std::move(displayMeshPSO);
	}
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
		mBackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	));

	if (!mBackBufferRtvAllocInfo->NumDescriptors)
	{
		mBackBufferRtvAllocInfo = descriptorManager->RtvAllocate(mBackBuffer.size());
	}

	for (int i = 0; i < mBackBuffer.size(); ++i)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(mBackBuffer[i]->GetAddressOf())));
		device->CreateRenderTargetView(mBackBuffer[i]->Get(), nullptr, descriptorManager->GetRtvHandle(mBackBufferRtvAllocInfo.get(), i));
	}
	
	mBackBufferIdx = 0;
}

void Carol::DisplayPass::InitSwapChain(HWND hwnd, IDXGIFactory* factory, ID3D12CommandQueue* cmdQueue)
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
	ThrowIfFailed(factory->CreateSwapChain(cmdQueue, &swapChainDesc, mSwapChain.GetAddressOf()));

}
