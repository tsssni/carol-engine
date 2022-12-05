#include "Taa.h"
#include "../Manager.h"
#include "../Display/Display.h"
#include "../Transparency/Oitppll.h"
#include "../../DirectX/Heap.h"
#include "../../DirectX/Resource.h"
#include "../../DirectX/Shader.h"
#include "../../DirectX/Sampler.h"
#include "../../Utils/Common.h"
#include <DirectXColors.h>
#include <cmath>

using std::make_unique;

Carol::TaaManager::TaaManager(
	RenderData* renderData,
	DXGI_FORMAT velocityMapFormat,
	DXGI_FORMAT frameFormat)
	:Manager(renderData),mVelocityMapFormat(velocityMapFormat),mFrameFormat(frameFormat)
{
	InitHalton();
	InitRootSignature();
	InitShaders();
	InitPSOs();
	OnResize();
}

void Carol::TaaManager::Update()
{
}

void Carol::TaaManager::OnResize()
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

void Carol::TaaManager::ReleaseIntermediateBuffers()
{
}

void Carol::TaaManager::InitRootSignature()
{
	CD3DX12_ROOT_PARAMETER rootPara[3];
	rootPara[0].InitAsConstantBufferView(0);

	CD3DX12_DESCRIPTOR_RANGE range[2];
	range[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	range[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 1);

	rootPara[1].InitAsDescriptorTable(1, &range[0], D3D12_SHADER_VISIBILITY_PIXEL);
	rootPara[2].InitAsDescriptorTable(1, &range[1], D3D12_SHADER_VISIBILITY_PIXEL);
	
	auto staticSamplers = StaticSampler::GetDefaultStaticSamplers();
	CD3DX12_ROOT_SIGNATURE_DESC slotRootSigDesc(3, rootPara, staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	Blob serializedRootSigBlob;
	Blob errorBlob;

	auto hr = D3D12SerializeRootSignature(&slotRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSigBlob.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob.Get())
	{
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	
	ThrowIfFailed(hr);
	ThrowIfFailed(mRenderData->Device->CreateRootSignature(0, serializedRootSigBlob->GetBufferPointer(), serializedRootSigBlob->GetBufferSize(), IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void Carol::TaaManager::InitShaders()
{
	const D3D_SHADER_MACRO skinnedDefines[] =
	{
		"SKINNED","1",
		NULL,NULL
	};

	(*mRenderData->Shaders)[L"TaaVelocityStaticVS"] = make_unique<Shader>(L"Shaders\\Velocity.hlsl", nullptr, "VS", "vs_5_1");
	(*mRenderData->Shaders)[L"TaaVelocityStaticPS"] = make_unique<Shader>(L"Shaders\\Velocity.hlsl", nullptr, "PS", "ps_5_1");
	(*mRenderData->Shaders)[L"TaaVelocitySkinnedVS"] = make_unique<Shader>(L"Shaders\\Velocity.hlsl", skinnedDefines, "VS", "vs_5_1");
	(*mRenderData->Shaders)[L"TaaVelocitySkinnedPS"] = make_unique<Shader>(L"Shaders\\Velocity.hlsl", skinnedDefines, "PS", "ps_5_1");
	(*mRenderData->Shaders)[L"TaaOutputVS"] = make_unique<Shader>(L"Shaders\\Taa.hlsl", nullptr, "VS", "vs_5_1");
	(*mRenderData->Shaders)[L"TaaOutputPS"] = make_unique<Shader>(L"Shaders\\Taa.hlsl", nullptr, "PS", "ps_5_1");
	
}

void Carol::TaaManager::InitPSOs()
{
	vector<D3D12_INPUT_ELEMENT_DESC> nullInputLayout(0);

	auto velocityStaticPsoDesc = *mRenderData->BasePsoDesc;
	auto velocityStaticVSBlob = (*mRenderData->Shaders)[L"TaaVelocityStaticVS"]->GetBlob()->Get();
	auto velocityStaticPSBlob = (*mRenderData->Shaders)[L"TaaVelocityStaticPS"]->GetBlob()->Get();
	velocityStaticPsoDesc.VS = { reinterpret_cast<byte*>(velocityStaticVSBlob->GetBufferPointer()),velocityStaticVSBlob->GetBufferSize() };
	velocityStaticPsoDesc.PS = { reinterpret_cast<byte*>(velocityStaticPSBlob->GetBufferPointer()),velocityStaticPSBlob->GetBufferSize() };
	velocityStaticPsoDesc.RTVFormats[0] = mVelocityMapFormat;
	ThrowIfFailed(mRenderData->Device->CreateGraphicsPipelineState(&velocityStaticPsoDesc, IID_PPV_ARGS((*mRenderData->PSOs)[L"TaaVelocityStatic"].GetAddressOf())));

	auto velocitySkinnedPsoDesc = velocityStaticPsoDesc;
	auto velocitySkinnedVSBlob = (*mRenderData->Shaders)[L"TaaVelocitySkinnedVS"]->GetBlob()->Get();
	auto velocitySkinnedPSBlob = (*mRenderData->Shaders)[L"TaaVelocitySkinnedPS"]->GetBlob()->Get();
	velocitySkinnedPsoDesc.VS = { reinterpret_cast<byte*>(velocitySkinnedVSBlob->GetBufferPointer()),velocitySkinnedVSBlob->GetBufferSize() };
	velocitySkinnedPsoDesc.PS = { reinterpret_cast<byte*>(velocitySkinnedPSBlob->GetBufferPointer()),velocitySkinnedPSBlob->GetBufferSize() };
	ThrowIfFailed(mRenderData->Device->CreateGraphicsPipelineState(&velocitySkinnedPsoDesc, IID_PPV_ARGS((*mRenderData->PSOs)[L"TaaVelocitySkinned"].GetAddressOf())));

	auto outputPsoDesc = *mRenderData->BasePsoDesc;
	outputPsoDesc.pRootSignature = mRootSignature.Get();
	auto outputVSBlob = (*mRenderData->Shaders)[L"TaaOutputVS"]->GetBlob()->Get();
	auto outputPSBlob = (*mRenderData->Shaders)[L"TaaOutputPS"]->GetBlob()->Get();
	outputPsoDesc.VS = { reinterpret_cast<byte*>(outputVSBlob->GetBufferPointer()),outputVSBlob->GetBufferSize() };
	outputPsoDesc.PS = { reinterpret_cast<byte*>(outputPSBlob->GetBufferPointer()),outputPSBlob->GetBufferSize() };
	outputPsoDesc.InputLayout = { nullInputLayout.data(),(uint32_t)nullInputLayout.size() };
	outputPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	outputPsoDesc.DepthStencilState.DepthEnable = false;
	ThrowIfFailed(mRenderData->Device->CreateGraphicsPipelineState(&outputPsoDesc, IID_PPV_ARGS((*mRenderData->PSOs)[L"TaaOutput"].GetAddressOf())));
}

void Carol::TaaManager::Draw()
{
	DrawCurrFrameMap();
	DrawVelocityMap();
	DrawOutput();
}

void Carol::TaaManager::DrawCurrFrameMap()
{
	mRenderData->CommandList->RSSetViewports(1, mRenderData->ScreenViewport);
	mRenderData->CommandList->RSSetScissorRects(1, mRenderData->ScissorRect);

	mRenderData->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCurrFrameMap->Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));
	mRenderData->CommandList->ClearRenderTargetView(GetRtv(CURR_RTV), DirectX::Colors::Gray, 0, nullptr);
	mRenderData->CommandList->ClearDepthStencilView(mRenderData->Display->GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mRenderData->CommandList->OMSetRenderTargets(1, GetRvaluePtr(GetRtv(CURR_RTV)), true, GetRvaluePtr(mRenderData->Display->GetDepthStencilView()));

	mRenderData->CommandList->SetGraphicsRootSignature(mRenderData->RootSignature);
	mRenderData->CommandList->SetGraphicsRootConstantBufferView(0, mRenderData->PassCBHeap->GetGPUVirtualAddress(mRenderData->PassCBAllocInfo));
	mRenderData->DrawOpaqueMeshes((*mRenderData->PSOs)[L"OpaqueStatic"].Get(), (*mRenderData->PSOs)[L"OpaqueSkinned"].Get(), true);
	mRenderData->DrawSkyBox((*mRenderData->PSOs)[L"SkyBox"].Get());
	mRenderData->Oitppll->Draw();

	mRenderData->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCurrFrameMap->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
}

void Carol::TaaManager::DrawVelocityMap()
{
	mRenderData->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mVelocityMap->Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));
	mRenderData->CommandList->ClearRenderTargetView(GetRtv(VELOCITY_RTV), DirectX::Colors::Black, 0, nullptr);
	mRenderData->CommandList->ClearDepthStencilView(mRenderData->Display->GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mRenderData->CommandList->OMSetRenderTargets(1, GetRvaluePtr(GetRtv(VELOCITY_RTV)), true, GetRvaluePtr(mRenderData->Display->GetDepthStencilView()));

	mRenderData->DrawMeshes(
		(*mRenderData->PSOs)[L"TaaVelocityStatic"].Get(),
		(*mRenderData->PSOs)[L"TaaVelocitySkinned"].Get(),
		(*mRenderData->PSOs)[L"TaaVelocityStatic"].Get(),
		(*mRenderData->PSOs)[L"TaaVelocitySkinned"].Get(),
		false
	);
	mRenderData->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mVelocityMap->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
}

void Carol::TaaManager::DrawOutput()
{
	mRenderData->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mRenderData->Display->GetCurrBackBuffer()->Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)));
	mRenderData->CommandList->ClearRenderTargetView(mRenderData->Display->GetCurrBackBufferView(), DirectX::Colors::Gray, 0, nullptr);
	mRenderData->CommandList->OMSetRenderTargets(1, GetRvaluePtr(mRenderData->Display->GetCurrBackBufferView()), true, nullptr);

	mRenderData->CommandList->SetGraphicsRootSignature(mRootSignature.Get());
	mRenderData->CommandList->SetPipelineState((*mRenderData->PSOs)[L"TaaOutput"].Get());
	mRenderData->CommandList->SetGraphicsRootConstantBufferView(0, mRenderData->PassCBHeap->GetGPUVirtualAddress(mRenderData->PassCBAllocInfo));
	mRenderData->CommandList->SetGraphicsRootDescriptorTable(1, mRenderData->Display->GetDepthStencilSrv());
	mRenderData->CommandList->SetGraphicsRootDescriptorTable(2, GetShaderGpuSrv(0));

	mRenderData->CommandList->IASetVertexBuffers(0, 0, nullptr);
	mRenderData->CommandList->IASetIndexBuffer(nullptr);
	mRenderData->CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mRenderData->CommandList->DrawInstanced(6, 1, 0, 0);

	mRenderData->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mHistFrameMap->Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST)));
	mRenderData->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mRenderData->Display->GetCurrBackBuffer()->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE)));
	mRenderData->CommandList->CopyResource(mHistFrameMap->Get(), mRenderData->Display->GetCurrBackBuffer()->Get());
	mRenderData->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mHistFrameMap->Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ)));
	mRenderData->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mRenderData->Display->GetCurrBackBuffer()->Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT)));
}

void Carol::TaaManager::GetHalton(float& proj0, float& proj1)
{
	static int i = 0;

	proj0 = (2 * mHalton[i].x - 1) / (*mRenderData->ClientWidth);
	proj1 = (2 * mHalton[i].y - 1) / (*mRenderData->ClientHeight);

	i = (i + 1) % 8;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::TaaManager::GetCurrFrameRtv()
{
	return GetRtv(CURR_RTV);
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
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = *mRenderData->ClientWidth;
    texDesc.Height = *mRenderData->ClientHeight;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = mFrameFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	mHistFrameMap = make_unique<DefaultResource>(&texDesc, mRenderData->SrvTexturesHeap, D3D12_RESOURCE_STATE_GENERIC_READ);

	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    CD3DX12_CLEAR_VALUE frameMapClearValue(mFrameFormat, DirectX::Colors::Gray);
	mCurrFrameMap = make_unique<DefaultResource>(&texDesc, mRenderData->RtvDsvTexturesHeap, D3D12_RESOURCE_STATE_GENERIC_READ, &frameMapClearValue);
	
    texDesc.Format = mVelocityMapFormat;
    CD3DX12_CLEAR_VALUE velocityMapClearValue(mVelocityMapFormat, DirectX::Colors::Black);
	mVelocityMap = make_unique<DefaultResource>(&texDesc, mRenderData->RtvDsvTexturesHeap, D3D12_RESOURCE_STATE_GENERIC_READ, &velocityMapClearValue);

	InitDescriptors();
}

void Carol::TaaManager::InitDescriptors()
{
	mRenderData->CbvSrvUavAllocator->CpuAllocate(TAA_SRV_COUNT, mCpuCbvSrvUavAllocInfo.get());
	mRenderData->RtvAllocator->CpuAllocate(TAA_RTV_COUNT, mRtvAllocInfo.get());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Format = mFrameFormat;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

	mRenderData->Device->CreateShaderResourceView(mHistFrameMap->Get(), &srvDesc, GetCpuSrv(HIST_SRV));
	mRenderData->Device->CreateShaderResourceView(mCurrFrameMap->Get(), &srvDesc, GetCpuSrv(CURR_SRV));	

	srvDesc.Format = mVelocityMapFormat;
	mRenderData->Device->CreateShaderResourceView(mVelocityMap->Get(), &srvDesc, GetCpuSrv(VELOCITY_SRV));

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Format = mFrameFormat;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;

	mRenderData->Device->CreateRenderTargetView(mCurrFrameMap->Get(), &rtvDesc, GetRtv(CURR_RTV));

	rtvDesc.Format = mVelocityMapFormat;
	mRenderData->Device->CreateRenderTargetView(mVelocityMap->Get(), &rtvDesc, GetRtv(VELOCITY_RTV));
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
