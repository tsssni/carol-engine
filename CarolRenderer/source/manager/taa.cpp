#include <manager/taa.h>
#include <global_resources.h>
#include <manager/display.h>
#include <manager/oitppll.h>
#include <dx12/heap.h>
#include <dx12/resource.h>
#include <dx12/shader.h>
#include <dx12/sampler.h>
#include <dx12/descriptor_allocator.h>
#include <utils/common.h>
#include <DirectXColors.h>
#include <cmath>

namespace Carol {
	using std::vector;
	using std::wstring;
	using std::make_unique;
}

Carol::TaaManager::TaaManager(
	GlobalResources* globalResources,
	DXGI_FORMAT velocityMapFormat,
	DXGI_FORMAT frameFormat)
	:Manager(globalResources),mVelocityMapFormat(velocityMapFormat),mFrameFormat(frameFormat)
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

	if (width != *mGlobalResources->ClientWidth || height != *mGlobalResources->ClientHeight)
	{
		Manager::OnResize();

		width = *mGlobalResources->ClientWidth;
		height = *mGlobalResources->ClientHeight;

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
	ThrowIfFailed(mGlobalResources->Device->CreateRootSignature(0, serializedRootSigBlob->GetBufferPointer(), serializedRootSigBlob->GetBufferSize(), IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void Carol::TaaManager::InitShaders()
{
	vector<wstring> nullDefines{};

	vector<wstring> skinnedDefines =
	{
		L"SKINNED=1"
	};

	(*mGlobalResources->Shaders)[L"TaaVelocityStaticVS"] = make_unique<Shader>(L"shader\\velocity.hlsl", nullDefines, L"VS", L"vs_6_5");
	(*mGlobalResources->Shaders)[L"TaaVelocityStaticPS"] = make_unique<Shader>(L"shader\\velocity.hlsl", nullDefines, L"PS", L"ps_6_5");
	(*mGlobalResources->Shaders)[L"TaaVelocitySkinnedVS"] = make_unique<Shader>(L"shader\\velocity.hlsl", skinnedDefines, L"VS", L"vs_6_5");
	(*mGlobalResources->Shaders)[L"TaaVelocitySkinnedPS"] = make_unique<Shader>(L"shader\\velocity.hlsl", skinnedDefines, L"PS", L"ps_6_5");
	(*mGlobalResources->Shaders)[L"TaaOutputVS"] = make_unique<Shader>(L"shader\\taa.hlsl", nullDefines, L"VS", L"vs_6_5");
	(*mGlobalResources->Shaders)[L"TaaOutputPS"] = make_unique<Shader>(L"shader\\taa.hlsl", nullDefines, L"PS", L"ps_6_5");
	
}

void Carol::TaaManager::InitPSOs()
{
	vector<D3D12_INPUT_ELEMENT_DESC> nullInputLayout(0);

	auto velocityStaticPsoDesc = *mGlobalResources->BasePsoDesc;
	auto velocityStaticVS = (*mGlobalResources->Shaders)[L"TaaVelocityStaticVS"].get();
	auto velocityStaticPS = (*mGlobalResources->Shaders)[L"TaaVelocityStaticPS"].get();
	velocityStaticPsoDesc.VS = { reinterpret_cast<byte*>(velocityStaticVS->GetBufferPointer()),velocityStaticVS->GetBufferSize() };
	velocityStaticPsoDesc.PS = { reinterpret_cast<byte*>(velocityStaticPS->GetBufferPointer()),velocityStaticPS->GetBufferSize() };
	velocityStaticPsoDesc.RTVFormats[0] = mVelocityMapFormat;
	ThrowIfFailed(mGlobalResources->Device->CreateGraphicsPipelineState(&velocityStaticPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"TaaVelocityStatic"].GetAddressOf())));

	auto velocitySkinnedPsoDesc = velocityStaticPsoDesc;
	auto velocitySkinnedVS = (*mGlobalResources->Shaders)[L"TaaVelocitySkinnedVS"].get();
	auto velocitySkinnedPS = (*mGlobalResources->Shaders)[L"TaaVelocitySkinnedPS"].get();
	velocitySkinnedPsoDesc.VS = { reinterpret_cast<byte*>(velocitySkinnedVS->GetBufferPointer()),velocitySkinnedVS->GetBufferSize() };
	velocitySkinnedPsoDesc.PS = { reinterpret_cast<byte*>(velocitySkinnedPS->GetBufferPointer()),velocitySkinnedPS->GetBufferSize() };
	ThrowIfFailed(mGlobalResources->Device->CreateGraphicsPipelineState(&velocitySkinnedPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"TaaVelocitySkinned"].GetAddressOf())));

	auto outputPsoDesc = *mGlobalResources->BasePsoDesc;
	outputPsoDesc.pRootSignature = mRootSignature.Get();
	auto outputVS = (*mGlobalResources->Shaders)[L"TaaOutputVS"].get();
	auto outputPS = (*mGlobalResources->Shaders)[L"TaaOutputPS"].get();
	outputPsoDesc.VS = { reinterpret_cast<byte*>(outputVS->GetBufferPointer()),outputVS->GetBufferSize() };
	outputPsoDesc.PS = { reinterpret_cast<byte*>(outputPS->GetBufferPointer()),outputPS->GetBufferSize() };
	outputPsoDesc.InputLayout = { nullInputLayout.data(),(uint32_t)nullInputLayout.size() };
	outputPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	outputPsoDesc.DepthStencilState.DepthEnable = false;
	ThrowIfFailed(mGlobalResources->Device->CreateGraphicsPipelineState(&outputPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"TaaOutput"].GetAddressOf())));
}

void Carol::TaaManager::Draw()
{
	DrawCurrFrameMap();
	DrawVelocityMap();
	DrawOutput();
}

void Carol::TaaManager::DrawCurrFrameMap()
{
	mGlobalResources->CommandList->RSSetViewports(1, mGlobalResources->ScreenViewport);
	mGlobalResources->CommandList->RSSetScissorRects(1, mGlobalResources->ScissorRect);

	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCurrFrameMap->Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));
	mGlobalResources->CommandList->ClearRenderTargetView(GetRtv(CURR_RTV), DirectX::Colors::Gray, 0, nullptr);
	mGlobalResources->CommandList->ClearDepthStencilView(mGlobalResources->Display->GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mGlobalResources->CommandList->OMSetRenderTargets(1, GetRvaluePtr(GetRtv(CURR_RTV)), true, GetRvaluePtr(mGlobalResources->Display->GetDepthStencilView()));

	mGlobalResources->CommandList->SetGraphicsRootSignature(mGlobalResources->RootSignature);
	mGlobalResources->CommandList->SetGraphicsRootConstantBufferView(0, mGlobalResources->PassCBHeap->GetGPUVirtualAddress(mGlobalResources->PassCBAllocInfo));
	mGlobalResources->DrawOpaqueMeshes((*mGlobalResources->PSOs)[L"OpaqueStatic"].Get(), (*mGlobalResources->PSOs)[L"OpaqueSkinned"].Get(), true);
	mGlobalResources->DrawSkyBox((*mGlobalResources->PSOs)[L"SkyBox"].Get());
	mGlobalResources->Oitppll->Draw();

	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCurrFrameMap->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
}

void Carol::TaaManager::DrawVelocityMap()
{
	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mVelocityMap->Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));
	mGlobalResources->CommandList->ClearRenderTargetView(GetRtv(VELOCITY_RTV), DirectX::Colors::Black, 0, nullptr);
	mGlobalResources->CommandList->ClearDepthStencilView(mGlobalResources->Display->GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mGlobalResources->CommandList->OMSetRenderTargets(1, GetRvaluePtr(GetRtv(VELOCITY_RTV)), true, GetRvaluePtr(mGlobalResources->Display->GetDepthStencilView()));

	mGlobalResources->DrawMeshes(
		(*mGlobalResources->PSOs)[L"TaaVelocityStatic"].Get(),
		(*mGlobalResources->PSOs)[L"TaaVelocitySkinned"].Get(),
		(*mGlobalResources->PSOs)[L"TaaVelocityStatic"].Get(),
		(*mGlobalResources->PSOs)[L"TaaVelocitySkinned"].Get(),
		false
	);
	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mVelocityMap->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
}

void Carol::TaaManager::DrawOutput()
{
	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mGlobalResources->Display->GetCurrBackBuffer()->Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)));
	mGlobalResources->CommandList->ClearRenderTargetView(mGlobalResources->Display->GetCurrBackBufferView(), DirectX::Colors::Gray, 0, nullptr);
	mGlobalResources->CommandList->OMSetRenderTargets(1, GetRvaluePtr(mGlobalResources->Display->GetCurrBackBufferView()), true, nullptr);

	mGlobalResources->CommandList->SetGraphicsRootSignature(mRootSignature.Get());
	mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"TaaOutput"].Get());
	mGlobalResources->CommandList->SetGraphicsRootConstantBufferView(0, mGlobalResources->PassCBHeap->GetGPUVirtualAddress(mGlobalResources->PassCBAllocInfo));
	mGlobalResources->CommandList->SetGraphicsRootDescriptorTable(1, mGlobalResources->Display->GetDepthStencilSrv());
	mGlobalResources->CommandList->SetGraphicsRootDescriptorTable(2, GetShaderGpuSrv(0));

	mGlobalResources->CommandList->IASetVertexBuffers(0, 0, nullptr);
	mGlobalResources->CommandList->IASetIndexBuffer(nullptr);
	mGlobalResources->CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mGlobalResources->CommandList->DrawInstanced(6, 1, 0, 0);

	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mHistFrameMap->Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST)));
	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mGlobalResources->Display->GetCurrBackBuffer()->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE)));
	mGlobalResources->CommandList->CopyResource(mHistFrameMap->Get(), mGlobalResources->Display->GetCurrBackBuffer()->Get());
	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mHistFrameMap->Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ)));
	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mGlobalResources->Display->GetCurrBackBuffer()->Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT)));
}

void Carol::TaaManager::GetHalton(float& proj0, float& proj1)
{
	static int i = 0;

	proj0 = (2 * mHalton[i].x - 1) / (*mGlobalResources->ClientWidth);
	proj1 = (2 * mHalton[i].y - 1) / (*mGlobalResources->ClientHeight);

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
    texDesc.Width = *mGlobalResources->ClientWidth;
    texDesc.Height = *mGlobalResources->ClientHeight;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = mFrameFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	mHistFrameMap = make_unique<DefaultResource>(&texDesc, mGlobalResources->TexturesHeap, D3D12_RESOURCE_STATE_GENERIC_READ);

	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    CD3DX12_CLEAR_VALUE frameMapClearValue(mFrameFormat, DirectX::Colors::Gray);
	mCurrFrameMap = make_unique<DefaultResource>(&texDesc, mGlobalResources->TexturesHeap, D3D12_RESOURCE_STATE_GENERIC_READ, &frameMapClearValue);
	
    texDesc.Format = mVelocityMapFormat;
    CD3DX12_CLEAR_VALUE velocityMapClearValue(mVelocityMapFormat, DirectX::Colors::Black);
	mVelocityMap = make_unique<DefaultResource>(&texDesc, mGlobalResources->TexturesHeap, D3D12_RESOURCE_STATE_GENERIC_READ, &velocityMapClearValue);

	InitDescriptors();
}

void Carol::TaaManager::InitDescriptors()
{
	mGlobalResources->CbvSrvUavAllocator->CpuAllocate(TAA_SRV_COUNT, mCpuCbvSrvUavAllocInfo.get());
	mGlobalResources->RtvAllocator->CpuAllocate(TAA_RTV_COUNT, mRtvAllocInfo.get());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Format = mFrameFormat;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

	mGlobalResources->Device->CreateShaderResourceView(mHistFrameMap->Get(), &srvDesc, GetCpuSrv(HIST_SRV));
	mGlobalResources->Device->CreateShaderResourceView(mCurrFrameMap->Get(), &srvDesc, GetCpuSrv(CURR_SRV));	

	srvDesc.Format = mVelocityMapFormat;
	mGlobalResources->Device->CreateShaderResourceView(mVelocityMap->Get(), &srvDesc, GetCpuSrv(VELOCITY_SRV));

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Format = mFrameFormat;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;

	mGlobalResources->Device->CreateRenderTargetView(mCurrFrameMap->Get(), &rtvDesc, GetRtv(CURR_RTV));

	rtvDesc.Format = mVelocityMapFormat;
	mGlobalResources->Device->CreateRenderTargetView(mVelocityMap->Get(), &rtvDesc, GetRtv(VELOCITY_RTV));
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
