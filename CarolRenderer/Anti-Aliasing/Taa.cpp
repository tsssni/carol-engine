#include "Taa.h"
#include "../FrameResources.h"
#include <DirectXColors.h>
#include <cmath>

using std::make_unique;

void Carol::TaaManager::InitTaa(ID3D12Device* device, uint32_t width, uint32_t height)
{
	mHistFrameMap = make_unique<DefaultBuffer>();
	mCurrFrameMap = make_unique<DefaultBuffer>();
	mVelocityMap = make_unique<DefaultBuffer>();

	mDevice = device;
	OnResize(width, height);
	InitHalton();
}

void Carol::TaaManager::OnResize(uint32_t width, uint32_t height)
{
	if (width != mRenderTargetWidth || height != mRenderTargetHeight)
	{
		mRenderTargetWidth = width;
		mRenderTargetHeight = height;

		InitResources();
	}
}

void Carol::TaaManager::InitDescriptors(Resource* depthStencilBuffer, CD3DX12_CPU_DESCRIPTOR_HANDLE cpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE gpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE cpuRtv, uint32_t cbvSrvUavDescriptorSize, uint32_t rtvDescriptorSize)
{
	mDepthMapCpuSrv = cpuSrv;
    mHistFrameMapCpuSrv = cpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    mCurrFrameMapCpuSrv = cpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    mVelocityMapCpuSrv = cpuSrv.Offset(1, cbvSrvUavDescriptorSize);

    mDepthMapGpuSrv = gpuSrv;
    mHistFrameMapGpuSrv = gpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    mCurrFrameMapGpuSrv = gpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    mVelocityMapGpuSrv  = gpuSrv.Offset(1, cbvSrvUavDescriptorSize);

    mCurrFrameMapCpuRtv = cpuRtv;
    mVelocityMapCpuRtv = cpuRtv.Offset(1, rtvDescriptorSize);

    RebuildDescriptors(depthStencilBuffer);
}

Carol::DefaultBuffer* Carol::TaaManager::GetHistFrameMap()
{
	return mHistFrameMap.get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::TaaManager::GetHistFrameMapSrv()
{
	return mHistFrameMapGpuSrv;
}

Carol::DefaultBuffer* Carol::TaaManager::GetCurrFrameMap()
{
	return mCurrFrameMap.get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::TaaManager::GetCurrFrameMapSrv()
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::TaaManager::GetCurrFrameMapRtv()
{
	return mCurrFrameMapCpuRtv;
}

Carol::DefaultBuffer* Carol::TaaManager::GetVelocityMap()
{
	return mVelocityMap.get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::TaaManager::GetVelocityMapSrv()
{
	return mVelocityMapGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::TaaManager::GetVelocityMapRtv()
{
	return mVelocityMapCpuRtv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::TaaManager::GetDepthSrv()
{
	return mDepthMapGpuSrv;
}

uint32_t Carol::TaaManager::GetNumSrvs()
{
	return mNumSrvs;
}

uint32_t Carol::TaaManager::GetNumRtvs()
{
	return mNumRtvs;
}

void Carol::TaaManager::GetHalton(float& proj0, float& proj1, uint32_t width, uint32_t height)
{
	static int i = 0;

	proj0 = (2 * mHalton[i].x - 1) / width;
	proj1 = (2 * mHalton[i].y - 1) / height;

	i = (i + 1) % 8;
}

void Carol::TaaManager::SetHistViewProj(DirectX::XMMATRIX& histViewProj)
{
	mHistViewProj = histViewProj;
}

DirectX::XMMATRIX Carol::TaaManager::GetHistViewProj()
{
	return mHistViewProj;
}

void Carol::TaaManager::InitResources()
{
    D3D12_RESOURCE_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = mRenderTargetWidth;
    texDesc.Height = mRenderTargetHeight;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = mFrameFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    CD3DX12_CLEAR_VALUE frameMapClearValue(mFrameFormat, DirectX::Colors::Gray);
    mCurrFrameMap->InitDefaultBuffer(mDevice, D3D12_HEAP_FLAG_NONE, &texDesc, &frameMapClearValue);
	mHistFrameMap->InitDefaultBuffer(mDevice, D3D12_HEAP_FLAG_NONE, &texDesc, &frameMapClearValue);
	
    texDesc.Format = mVelocityMapFormat;
    CD3DX12_CLEAR_VALUE velocityMapClearValue(mVelocityMapFormat, DirectX::Colors::Black);
    mVelocityMap->InitDefaultBuffer(mDevice, D3D12_HEAP_FLAG_NONE, &texDesc, &velocityMapClearValue);
}

void Carol::TaaManager::RebuildDescriptors(Resource* depthStencilBuffer)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
	mDevice->CreateShaderResourceView(depthStencilBuffer->Get(), &srvDesc, mDepthMapCpuSrv);

	srvDesc.Format = mFrameFormat;
	mDevice->CreateShaderResourceView(mHistFrameMap->Get(), &srvDesc, mHistFrameMapCpuSrv);
	mDevice->CreateShaderResourceView(mCurrFrameMap->Get(), &srvDesc, mCurrFrameMapCpuSrv);

	srvDesc.Format = mVelocityMapFormat;
	mDevice->CreateShaderResourceView(mVelocityMap->Get(), &srvDesc, mVelocityMapCpuSrv);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Format = mFrameFormat;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;
	mDevice->CreateRenderTargetView(mCurrFrameMap->Get(), &rtvDesc, mCurrFrameMapCpuRtv);

	rtvDesc.Format = mVelocityMapFormat;
	mDevice->CreateRenderTargetView(mVelocityMap->Get(), &rtvDesc, mVelocityMapCpuRtv);
}

void Carol::TaaManager::InitHalton()
{
	for (int i = 0; i < 8; ++i)
	{
		mHalton[i].x = RadicalInversion(2, i + 1);
		mHalton[i].y = RadicalInversion(3, i + 1);
	}
}

float Carol::TaaManager::RadicalInversion(int base, int num)
{
	int temp[4];
	int i = 0;

	while (num != 0)
	{
		temp[i++] = num % base;
		num /= base;
	}

	double convertionResult = 0;
	for (int j = 0; j < i; ++j)
	{
		convertionResult += temp[j] * pow(base, -j - 1);
	}

	return convertionResult;
}

void Carol::TaaManager::SetFrameFormat(DXGI_FORMAT format)
{
	mFrameFormat = format;
}

DXGI_FORMAT Carol::TaaManager::GetFrameFormat()
{
	return mFrameFormat;
}

void Carol::TaaManager::SetVelocityFormat(DXGI_FORMAT format)
{
	mVelocityMapFormat = format;
}

DXGI_FORMAT Carol::TaaManager::GetVelocityFormat()
{
	return mVelocityMapFormat;
}
