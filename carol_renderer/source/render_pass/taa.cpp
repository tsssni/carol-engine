#include <render_pass/taa.h>
#include <render_pass/global_resources.h>
#include <render_pass/display.h>
#include <render_pass/frame.h>
#include <render_pass/shadow.h>
#include <render_pass/oitppll.h>
#include <render_pass/ssao.h>
#include <render_pass/mesh.h>
#include <dx12/heap.h>
#include <dx12/resource.h>
#include <dx12/shader.h>
#include <dx12/sampler.h>
#include <dx12/descriptor_allocator.h>
#include <dx12/root_signature.h>
#include <utils/common.h>
#include <DirectXColors.h>
#include <cmath>

namespace Carol {
	using std::vector;
	using std::wstring;
	using std::make_unique;
}

Carol::TaaPass::TaaPass(
	GlobalResources* globalResources,
	DXGI_FORMAT velocityMapFormat,
	DXGI_FORMAT frameFormat)
	:RenderPass(globalResources),
	 mVelocityMapFormat(velocityMapFormat),
	 mFrameFormat(frameFormat)
{
	InitHalton();
	InitShaders();
	InitPSOs();
	OnResize();
}

void Carol::TaaPass::Update()
{
}

void Carol::TaaPass::OnResize()
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

void Carol::TaaPass::ReleaseIntermediateBuffers()
{
}

void Carol::TaaPass::InitShaders()
{
	vector<wstring> nullDefines{};

	vector<wstring> skinnedDefines =
	{
		L"SKINNED=1"
	};

	(*mGlobalResources->Shaders)[L"TaaVelocityStaticVS"] = make_unique<Shader>(L"shader\\velocity.hlsl", nullDefines, L"VS", L"vs_6_6");
	(*mGlobalResources->Shaders)[L"TaaVelocityStaticPS"] = make_unique<Shader>(L"shader\\velocity.hlsl", nullDefines, L"PS", L"ps_6_6");
	(*mGlobalResources->Shaders)[L"TaaVelocitySkinnedVS"] = make_unique<Shader>(L"shader\\velocity.hlsl", skinnedDefines, L"VS", L"vs_6_6");
	(*mGlobalResources->Shaders)[L"TaaVelocitySkinnedPS"] = make_unique<Shader>(L"shader\\velocity.hlsl", skinnedDefines, L"PS", L"ps_6_6");
	(*mGlobalResources->Shaders)[L"TaaOutputVS"] = make_unique<Shader>(L"shader\\taa.hlsl", nullDefines, L"VS", L"vs_6_6");
	(*mGlobalResources->Shaders)[L"TaaOutputPS"] = make_unique<Shader>(L"shader\\taa.hlsl", nullDefines, L"PS", L"ps_6_6");
}

void Carol::TaaPass::InitPSOs()
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
	auto outputVS = (*mGlobalResources->Shaders)[L"TaaOutputVS"].get();
	auto outputPS = (*mGlobalResources->Shaders)[L"TaaOutputPS"].get();
	outputPsoDesc.VS = { reinterpret_cast<byte*>(outputVS->GetBufferPointer()),outputVS->GetBufferSize() };
	outputPsoDesc.PS = { reinterpret_cast<byte*>(outputPS->GetBufferPointer()),outputPS->GetBufferSize() };
	outputPsoDesc.InputLayout = { nullInputLayout.data(),(uint32_t)nullInputLayout.size() };
	outputPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	outputPsoDesc.DepthStencilState.DepthEnable = false;
	ThrowIfFailed(mGlobalResources->Device->CreateGraphicsPipelineState(&outputPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"TaaOutput"].GetAddressOf())));
}

void Carol::TaaPass::Draw()
{
	DrawVelocityMap();
	DrawOutput();
}


void Carol::TaaPass::DrawVelocityMap()
{
	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mVelocityMap->Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));
	mGlobalResources->CommandList->ClearRenderTargetView(GetRtv(VELOCITY_RTV), DirectX::Colors::Black, 0, nullptr);
	mGlobalResources->CommandList->ClearDepthStencilView(mGlobalResources->Frame->GetDepthStencilDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mGlobalResources->CommandList->OMSetRenderTargets(1, GetRvaluePtr(GetRtv(VELOCITY_RTV)), true, GetRvaluePtr(mGlobalResources->Frame->GetDepthStencilDsv()));

	mGlobalResources->Meshes->DrawMeshes({
		(*mGlobalResources->PSOs)[L"TaaVelocityStatic"].Get(),
		(*mGlobalResources->PSOs)[L"TaaVelocitySkinned"].Get(),
		(*mGlobalResources->PSOs)[L"TaaVelocityStatic"].Get(),
		(*mGlobalResources->PSOs)[L"TaaVelocitySkinned"].Get()
		});
	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mVelocityMap->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
}

void Carol::TaaPass::DrawOutput()
{
	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mGlobalResources->Display->GetCurrBackBuffer()->Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)));
	mGlobalResources->CommandList->ClearRenderTargetView(mGlobalResources->Display->GetCurrBackBufferRtv(), DirectX::Colors::Gray, 0, nullptr);
	mGlobalResources->CommandList->OMSetRenderTargets(1, GetRvaluePtr(mGlobalResources->Display->GetCurrBackBufferRtv()), true, nullptr);
	mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"TaaOutput"].Get());

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

void Carol::TaaPass::GetHalton(float& proj0, float& proj1)
{
	static int i = 0;

	proj0 = (2 * mHalton[i].x - 1) / (*mGlobalResources->ClientWidth);
	proj1 = (2 * mHalton[i].y - 1) / (*mGlobalResources->ClientHeight);

	i = (i + 1) % 8;
}

void Carol::TaaPass::SetHistViewProj(DirectX::XMMATRIX& histViewProj)
{
	mHistViewProj = histViewProj;
}

DirectX::XMMATRIX Carol::TaaPass::GetHistViewProj()
{
	return mHistViewProj;
}

uint32_t Carol::TaaPass::GetVeloctiySrvIdx()
{
	return mGpuCbvSrvUavAllocInfo->StartOffset + VELOCITY_SRV;
}

uint32_t Carol::TaaPass::GetHistFrameSrvIdx()
{
	return mGpuCbvSrvUavAllocInfo->StartOffset + HIST_SRV;
}

void Carol::TaaPass::InitResources()
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
	
    texDesc.Format = mVelocityMapFormat;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    CD3DX12_CLEAR_VALUE velocityMapClearValue(mVelocityMapFormat, DirectX::Colors::Black);
	mVelocityMap = make_unique<DefaultResource>(&texDesc, mGlobalResources->TexturesHeap, D3D12_RESOURCE_STATE_GENERIC_READ, &velocityMapClearValue);

	InitDescriptors();
}

void Carol::TaaPass::InitDescriptors()
{
	mGlobalResources->CbvSrvUavAllocator->CpuAllocate(TAA_CBV_SRV_UAV_COUNT, mCpuCbvSrvUavAllocInfo.get());
	mGlobalResources->RtvAllocator->CpuAllocate(TAA_RTV_COUNT, mRtvAllocInfo.get());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = mFrameFormat;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	mGlobalResources->Device->CreateShaderResourceView(mHistFrameMap->Get(), &srvDesc, GetCpuCbvSrvUav(HIST_SRV));

	srvDesc.Format = mVelocityMapFormat;
	mGlobalResources->Device->CreateShaderResourceView(mVelocityMap->Get(), &srvDesc, GetCpuCbvSrvUav(VELOCITY_SRV));

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Format = mVelocityMapFormat;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;

	mGlobalResources->Device->CreateRenderTargetView(mVelocityMap->Get(), &rtvDesc, GetRtv(VELOCITY_RTV));

	CopyDescriptors();
}

void Carol::TaaPass::InitHalton()
{
	for (int i = 0; i < 8; ++i)
	{
		mHalton[i].x = RadicalInversion(2, i + 1);
		mHalton[i].y = RadicalInversion(3, i + 1);
	}
}

float Carol::TaaPass::RadicalInversion(int base, int num)
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
