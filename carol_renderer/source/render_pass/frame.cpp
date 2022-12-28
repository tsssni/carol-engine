#include <render_pass/frame.h>
#include <render_pass/global_resources.h>
#include <render_pass/shadow.h>
#include <render_pass/ssao.h>
#include <render_pass/mesh.h>
#include <render_pass/oitppll.h>
#include <render_pass/taa.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/root_signature.h>
#include <dx12/descriptor_allocator.h>
#include <dx12/shader.h>
#include <scene/camera.h>
#include <DirectXColors.h>

namespace Carol
{
	using std::vector;
	using std::wstring;
    using std::make_unique;
	using namespace DirectX;
}

Carol::FramePass::FramePass(GlobalResources* globalResources, DXGI_FORMAT frameFormat,DXGI_FORMAT depthStencilResourceFormat,  DXGI_FORMAT depthStencilDsvFormat, DXGI_FORMAT depthStencilSrvFormat)
    :RenderPass(globalResources),
	mFrameFormat(frameFormat),
	mDepthStencilResourceFormat(depthStencilResourceFormat),
	mDepthStencilDsvFormat(depthStencilDsvFormat),
	mDepthStencilSrvFormat(depthStencilSrvFormat)
{
    InitShaders();
    InitPSOs();
    OnResize();
}

void Carol::FramePass::Draw()
{
	mGlobalResources->CommandList->RSSetViewports(1, mGlobalResources->ScreenViewport);
	mGlobalResources->CommandList->RSSetScissorRects(1, mGlobalResources->ScissorRect);

	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mFrameMap->Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET)));
	mGlobalResources->CommandList->ClearRenderTargetView(GetRtv(FRAME_RTV), Colors::Gray, 0, nullptr);
	mGlobalResources->CommandList->ClearDepthStencilView(GetDsv(DEPTH_STENCIL_DSV), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mGlobalResources->CommandList->OMSetRenderTargets(1, GetRvaluePtr(GetRtv(FRAME_RTV)), true, GetRvaluePtr(GetDsv(DEPTH_STENCIL_DSV)));
	
	mGlobalResources->Meshes->DrawMeshes({
		(*mGlobalResources->PSOs)[L"OpaqueStatic"].Get(),
		(*mGlobalResources->PSOs)[L"OpaqueSkinned"].Get(),
		nullptr,
		nullptr
		}, true);
	mGlobalResources->Meshes->DrawSkyBox((*mGlobalResources->PSOs)[L"SkyBox"].Get());
	mGlobalResources->Oitppll->Draw();

	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mFrameMap->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
}

void Carol::FramePass::Update()
{
    
}

void Carol::FramePass::OnResize()
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

void Carol::FramePass::ReleaseIntermediateBuffers()
{
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::FramePass::GetFrameRtv()
{
	return GetRtv(FRAME_RTV);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::FramePass::GetDepthStencilDsv()
{
    return GetDsv(DEPTH_STENCIL_DSV);
}

uint32_t Carol::FramePass::GetFrameSrvIdx()
{
	return mGpuCbvSrvUavAllocInfo->StartOffset + FRAME_SRV;
}

uint32_t Carol::FramePass::GetDepthStencilSrvIdx()
{
	return mGpuCbvSrvUavAllocInfo->StartOffset + DEPTH_STENCIL_SRV;
}

void Carol::FramePass::InitShaders()
{
	vector<wstring> staticDefines =
	{
		L"TAA=1",L"SSAO=1"
	};

	vector<wstring> skinnedDefines =
	{
		L"TAA=1",L"SSAO=1",L"SKINNED=1"
	};

	(*mGlobalResources->Shaders)[L"OpaqueStaticVS"] = make_unique<Shader>(L"shader\\default.hlsl", staticDefines, L"VS", L"vs_6_6");
	(*mGlobalResources->Shaders)[L"OpaqueStaticPS"] = make_unique<Shader>(L"shader\\default.hlsl", staticDefines, L"PS", L"ps_6_6");
	(*mGlobalResources->Shaders)[L"OpaqueSkinnedVS"] = make_unique<Shader>(L"shader\\default.hlsl", skinnedDefines, L"VS", L"vs_6_6");
	(*mGlobalResources->Shaders)[L"OpauqeSkinnedPS"] = make_unique<Shader>(L"shader\\default.hlsl", skinnedDefines, L"PS", L"ps_6_6");
	(*mGlobalResources->Shaders)[L"SkyBoxVS"] = make_unique<Shader>(L"shader\\skybox.hlsl", staticDefines, L"VS", L"vs_6_6");
	(*mGlobalResources->Shaders)[L"SkyBoxPS"] = make_unique<Shader>(L"shader\\skybox.hlsl", staticDefines, L"PS", L"ps_6_6");
}

void Carol::FramePass::InitPSOs()
{
	auto opaqueStaticPsoDesc = *mGlobalResources->BasePsoDesc;
	auto opaqueStaticVS = (*mGlobalResources->Shaders)[L"OpaqueStaticVS"].get();
	auto opaqueStaticPS = (*mGlobalResources->Shaders)[L"OpaqueStaticPS"].get();
	opaqueStaticPsoDesc.VS = { reinterpret_cast<byte*>(opaqueStaticVS->GetBufferPointer()),opaqueStaticVS->GetBufferSize() };
	opaqueStaticPsoDesc.PS = { reinterpret_cast<byte*>(opaqueStaticPS->GetBufferPointer()),opaqueStaticPS->GetBufferSize() };
	ThrowIfFailed(mGlobalResources->Device->CreateGraphicsPipelineState(&opaqueStaticPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"OpaqueStatic"].GetAddressOf())));

	auto opaqueSkinnedPsoDesc = opaqueStaticPsoDesc;
	auto opaqueSkinnedVS = (*mGlobalResources->Shaders)[L"OpaqueSkinnedVS"].get();
	auto opaqueSkinnedPS = (*mGlobalResources->Shaders)[L"OpauqeSkinnedPS"].get();
	opaqueSkinnedPsoDesc.VS = { reinterpret_cast<byte*>(opaqueSkinnedVS->GetBufferPointer()),opaqueSkinnedVS->GetBufferSize() };
	opaqueSkinnedPsoDesc.PS = { reinterpret_cast<byte*>(opaqueSkinnedPS->GetBufferPointer()),opaqueSkinnedPS->GetBufferSize() };	
	ThrowIfFailed(mGlobalResources->Device->CreateGraphicsPipelineState(&opaqueSkinnedPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"OpaqueSkinned"].GetAddressOf())));

	auto skyBoxPsoDesc = *mGlobalResources->BasePsoDesc;
	auto skyBoxVS = (*mGlobalResources->Shaders)[L"SkyBoxVS"].get();
	auto skyBoxPS = (*mGlobalResources->Shaders)[L"SkyBoxPS"].get();
	skyBoxPsoDesc.VS = { reinterpret_cast<byte*>(skyBoxVS->GetBufferPointer()),skyBoxVS->GetBufferSize() };
	skyBoxPsoDesc.PS = { reinterpret_cast<byte*>(skyBoxPS->GetBufferPointer()),skyBoxPS->GetBufferSize() };
	skyBoxPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	ThrowIfFailed(mGlobalResources->Device->CreateGraphicsPipelineState(&skyBoxPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"SkyBox"].GetAddressOf())));
}

void Carol::FramePass::InitResources()
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
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    CD3DX12_CLEAR_VALUE optClear(mFrameFormat, DirectX::Colors::Gray);
	mFrameMap = make_unique<DefaultResource>(&texDesc, mGlobalResources->TexturesHeap, D3D12_RESOURCE_STATE_GENERIC_READ, &optClear);

	texDesc.Format = mDepthStencilResourceFormat;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	optClear.Format = mDepthStencilDsvFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	mDepthStencilMap = make_unique<DefaultResource>(&texDesc, mGlobalResources->TexturesHeap, D3D12_RESOURCE_STATE_GENERIC_READ, &optClear);

	InitDescriptors();
}

void Carol::FramePass::InitDescriptors()
{
	mGlobalResources->CbvSrvUavAllocator->CpuAllocate(FRAME_CBV_SRV_UAV_COUNT, mCpuCbvSrvUavAllocInfo.get());
	mGlobalResources->RtvAllocator->CpuAllocate(FRAME_RTV_COUNT, mRtvAllocInfo.get());
	mGlobalResources->DsvAllocator->CpuAllocate(FRAME_DSV_COUNT, mDsvAllocInfo.get());
	
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = mFrameFormat;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	mGlobalResources->Device->CreateShaderResourceView(mFrameMap->Get(), &srvDesc, GetCpuCbvSrvUav(FRAME_SRV));

	srvDesc.Format = mDepthStencilSrvFormat;
	mGlobalResources->Device->CreateShaderResourceView(mDepthStencilMap->Get(), &srvDesc, GetCpuCbvSrvUav(DEPTH_STENCIL_SRV));
	
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Format = mFrameFormat;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;

	mGlobalResources->Device->CreateRenderTargetView(mFrameMap->Get(), &rtvDesc, GetRtv(FRAME_RTV));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = mDepthStencilDsvFormat;
	dsvDesc.Texture2D.MipSlice = 0;

	mGlobalResources->Device->CreateDepthStencilView(mDepthStencilMap->Get(), &dsvDesc, GetDsv(DEPTH_STENCIL_DSV));
	
	CopyDescriptors();
}