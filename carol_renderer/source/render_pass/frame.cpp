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

Carol::FramePass::FramePass(GlobalResources* globalResources, DXGI_FORMAT frameFormat,DXGI_FORMAT depthStencilResourceFormat,  DXGI_FORMAT depthStencilDsvFormat, DXGI_FORMAT depthStencilSrvFormat, DXGI_FORMAT hiZFormat)
    :RenderPass(globalResources),
	mCpuHiZUavAllocInfo(make_unique<DescriptorAllocInfo>()),
	mGpuHiZUavAllocInfo(make_unique<DescriptorAllocInfo>()),
	mFrameFormat(frameFormat),
	mDepthStencilResourceFormat(depthStencilResourceFormat),
	mDepthStencilDsvFormat(depthStencilDsvFormat),
	mDepthStencilSrvFormat(depthStencilSrvFormat),
	mHiZFormat(hiZFormat)
{
    InitShaders();
    InitPSOs();
}

void Carol::FramePass::Draw()
{
	DrawFrame();
	DrawHiZ();
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
		DeallocateDescriptors();

		width = *mGlobalResources->ClientWidth;
		height = *mGlobalResources->ClientHeight;
		mHiZMipLevel = std::min(floor(log2(width)), floor(log2(height)));

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

uint32_t Carol::FramePass::GetHiZSrvIdx()
{
	return mGpuCbvSrvUavAllocInfo->StartOffset + HIZ_SRV;
}

uint32_t Carol::FramePass::GetHiZUavIdx()
{
	return mGpuHiZUavAllocInfo->StartOffset;
}

void Carol::FramePass::InitShaders()
{
	vector<wstring> nullDefines = {};

	vector<wstring> staticDefines =
	{
		L"TAA=1",L"SSAO=1"
	};

	vector<wstring> skinnedDefines =
	{
		L"TAA=1",L"SSAO=1",L"SKINNED=1"
	};
	
	(*mGlobalResources->Shaders)[L"CullAS"] = make_unique<Shader>(L"shader\\cull_as.hlsl", nullDefines, L"main", L"as_6_6");
	(*mGlobalResources->Shaders)[L"OpaqueStaticMS"] = make_unique<Shader>(L"shader\\default_ms.hlsl", staticDefines, L"main", L"ms_6_6");
	(*mGlobalResources->Shaders)[L"OpaqueStaticPS"] = make_unique<Shader>(L"shader\\default_ps.hlsl", staticDefines, L"main", L"ps_6_6");
	(*mGlobalResources->Shaders)[L"OpaqueSkinnedMS"] = make_unique<Shader>(L"shader\\default_ms.hlsl", skinnedDefines, L"main", L"ms_6_6");
	(*mGlobalResources->Shaders)[L"OpaqueSkinnedPS"] = make_unique<Shader>(L"shader\\default_ps.hlsl", skinnedDefines, L"main", L"ps_6_6");
	(*mGlobalResources->Shaders)[L"ScreenMS"] = make_unique<Shader>(L"shader\\screen_ms.hlsl", nullDefines, L"main", L"ms_6_6");
	(*mGlobalResources->Shaders)[L"SkyBoxMS"] = make_unique<Shader>(L"shader\\skybox_ms.hlsl", staticDefines, L"main", L"ms_6_6");
	(*mGlobalResources->Shaders)[L"SkyBoxPS"] = make_unique<Shader>(L"shader\\skybox_ps.hlsl", staticDefines, L"main", L"ps_6_6");
	(*mGlobalResources->Shaders)[L"HiZGenerateCS"] = make_unique<Shader>(L"shader\\hiz_generate_cs.hlsl", nullDefines, L"main", L"cs_6_6");
}

void Carol::FramePass::InitPSOs()
{
	auto opaqueStaticPsoDesc = *mGlobalResources->BaseGraphicsPsoDesc;
	auto opaqueStaticAS = (*mGlobalResources->Shaders)[L"CullAS"].get();
	auto opaqueStaticMS = (*mGlobalResources->Shaders)[L"OpaqueStaticMS"].get();
	auto opaqueStaticPS = (*mGlobalResources->Shaders)[L"OpaqueStaticPS"].get();
	opaqueStaticPsoDesc.AS = { reinterpret_cast<byte*>(opaqueStaticAS->GetBufferPointer()),opaqueStaticAS->GetBufferSize() };
	opaqueStaticPsoDesc.MS = { reinterpret_cast<byte*>(opaqueStaticMS->GetBufferPointer()),opaqueStaticMS->GetBufferSize() };
	opaqueStaticPsoDesc.PS = { reinterpret_cast<byte*>(opaqueStaticPS->GetBufferPointer()),opaqueStaticPS->GetBufferSize() };
	auto opaqueStaticPsoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(opaqueStaticPsoDesc);
    D3D12_PIPELINE_STATE_STREAM_DESC opaqueStaticStreamDesc;
    opaqueStaticStreamDesc.pPipelineStateSubobjectStream = &opaqueStaticPsoStream;
    opaqueStaticStreamDesc.SizeInBytes = sizeof(opaqueStaticPsoStream);
    ThrowIfFailed(mGlobalResources->Device->CreatePipelineState(&opaqueStaticStreamDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"OpaqueStatic"].GetAddressOf())));

	auto opaqueSkinnedPsoDesc = opaqueStaticPsoDesc;
	auto opaqueSkinnedMS = (*mGlobalResources->Shaders)[L"OpaqueSkinnedMS"].get();
	auto opaqueSkinnedPS = (*mGlobalResources->Shaders)[L"OpaqueSkinnedPS"].get();
	opaqueSkinnedPsoDesc.MS = { reinterpret_cast<byte*>(opaqueSkinnedMS->GetBufferPointer()),opaqueSkinnedMS->GetBufferSize() };
	opaqueSkinnedPsoDesc.PS = { reinterpret_cast<byte*>(opaqueSkinnedPS->GetBufferPointer()),opaqueSkinnedPS->GetBufferSize() };
	auto opaqueSkinnedPsoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(opaqueSkinnedPsoDesc);
    D3D12_PIPELINE_STATE_STREAM_DESC opaqueSkinnedStreamDesc;
    opaqueSkinnedStreamDesc.pPipelineStateSubobjectStream = &opaqueSkinnedPsoStream;
    opaqueSkinnedStreamDesc.SizeInBytes = sizeof(opaqueSkinnedPsoStream);
    ThrowIfFailed(mGlobalResources->Device->CreatePipelineState(&opaqueSkinnedStreamDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"OpaqueSkinned"].GetAddressOf())));

	auto skyBoxPsoDesc = *mGlobalResources->BaseGraphicsPsoDesc;
	auto skyBoxMS = (*mGlobalResources->Shaders)[L"SkyBoxMS"].get();
	auto skyBoxPS = (*mGlobalResources->Shaders)[L"SkyBoxPS"].get();
	skyBoxPsoDesc.MS = { reinterpret_cast<byte*>(skyBoxMS->GetBufferPointer()),skyBoxMS->GetBufferSize() };
	skyBoxPsoDesc.PS = { reinterpret_cast<byte*>(skyBoxPS->GetBufferPointer()),skyBoxPS->GetBufferSize() };
	skyBoxPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	auto skyBoxPsoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(skyBoxPsoDesc);
    D3D12_PIPELINE_STATE_STREAM_DESC skyBoxStreamDesc;
    skyBoxStreamDesc.pPipelineStateSubobjectStream = &skyBoxPsoStream;
    skyBoxStreamDesc.SizeInBytes = sizeof(skyBoxPsoStream);
    ThrowIfFailed(mGlobalResources->Device->CreatePipelineState(&skyBoxStreamDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"SkyBox"].GetAddressOf())));

	auto hizGenratePsoDesc = *mGlobalResources->BaseComputePsoDesc;
	auto hizGenerateCS = (*mGlobalResources->Shaders)[L"HiZGenerateCS"].get();
	hizGenratePsoDesc.CS = { reinterpret_cast<byte*>(hizGenerateCS->GetBufferPointer()), hizGenerateCS->GetBufferSize() };
	ThrowIfFailed(mGlobalResources->Device->CreateComputePipelineState(&hizGenratePsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"HiZGenerate"].GetAddressOf())));
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
	
	mDepthStencilMap = make_unique<DefaultResource>(&texDesc, mGlobalResources->TexturesHeap, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	texDesc.MipLevels = mHiZMipLevel;
	texDesc.Format = mHiZFormat;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	mHiZMap = make_unique<DefaultResource>(&texDesc, mGlobalResources->TexturesHeap, D3D12_RESOURCE_STATE_COMMON);

	InitDescriptors();
}

void Carol::FramePass::InitDescriptors()
{
	mGlobalResources->CbvSrvUavAllocator->CpuAllocate(FRAME_CBV_SRV_UAV_COUNT, mCpuCbvSrvUavAllocInfo.get());
	mGlobalResources->RtvAllocator->CpuAllocate(FRAME_RTV_COUNT, mRtvAllocInfo.get());
	mGlobalResources->DsvAllocator->CpuAllocate(FRAME_DSV_COUNT, mDsvAllocInfo.get());
	mGlobalResources->CbvSrvUavAllocator->CpuAllocate(mHiZMipLevel, mCpuHiZUavAllocInfo.get());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = mFrameFormat;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	mGlobalResources->Device->CreateShaderResourceView(mFrameMap->Get(), &srvDesc, GetCpuCbvSrvUav(FRAME_SRV));

	srvDesc.Format = mDepthStencilSrvFormat;
	mGlobalResources->Device->CreateShaderResourceView(mDepthStencilMap->Get(), &srvDesc, GetCpuCbvSrvUav(DEPTH_STENCIL_SRV));

	srvDesc.Texture2D.MipLevels = mHiZMipLevel;
	srvDesc.Format = mHiZFormat;
	mGlobalResources->Device->CreateShaderResourceView(mHiZMap->Get(), &srvDesc, GetCpuCbvSrvUav(HIZ_SRV));

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

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = mHiZFormat;
	uavDesc.Texture2D.PlaneSlice = 0;

	for (int i = 0; i < mHiZMipLevel; ++i)
	{
		uavDesc.Texture2D.MipSlice = i;
		mGlobalResources->Device->CreateUnorderedAccessView(mHiZMap->Get(), nullptr, &uavDesc, GetCpuHiZUav(i));
	}

	CopyDescriptors();
	CopyHiZUav();
}

void Carol::FramePass::DeallocateDescriptors()
{
	RenderPass::DeallocateDescriptors();

	if (mCpuHiZUavAllocInfo->Allocator)
	{
		mGlobalResources->CbvSrvUavAllocator->CpuDeallocate(mCpuHiZUavAllocInfo.get());
	}

	if (mGpuHiZUavAllocInfo->Allocator)
	{
		mGlobalResources->CbvSrvUavAllocator->GpuDeallocate(mGpuHiZUavAllocInfo.get());
	}
}

void Carol::FramePass::DrawFrame()
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
		nullptr });
	mGlobalResources->Meshes->DrawSkyBox((*mGlobalResources->PSOs)[L"SkyBox"].Get());
	mGlobalResources->Oitppll->Draw();

	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mFrameMap->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ)));
}

void Carol::FramePass::DrawHiZ()
{
	for (int i = 0; i < mHiZMipLevel; i += 5)
	{
		uint32_t hiZConstants[2] = { i, i + 5 >= mHiZMipLevel ? mHiZMipLevel - i - 1 : 5 };
		mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"HiZGenerate"].Get());
		mGlobalResources->CommandList->SetComputeRoot32BitConstants(RootSignature::PASS_CONSTANTS, 2, hiZConstants, 0);
		
		uint32_t width = ceilf(((*mGlobalResources->ClientWidth) >> i) / 32.f);
		uint32_t height = ceilf(((*mGlobalResources->ClientHeight) >> i) / 32.f);
		mGlobalResources->CommandList->Dispatch(width, height, 1);
		mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::UAV(mHiZMap->Get())));
	}
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::FramePass::GetCpuHiZUav(uint32_t mipLevel)
{
	auto cpuHandle = mGlobalResources->CbvSrvUavAllocator->GetCpuHandle(mCpuHiZUavAllocInfo.get());
	cpuHandle.Offset(mipLevel * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return cpuHandle;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::FramePass::GetShaderCpuHiZUav(uint32_t mipLevel)
{
	auto shaderCpuHandle = mGlobalResources->CbvSrvUavAllocator->GetShaderCpuHandle(mGpuHiZUavAllocInfo.get());
	shaderCpuHandle.Offset(mipLevel * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderCpuHandle;
}

void Carol::FramePass::CopyHiZUav()
{
	mGlobalResources->CbvSrvUavAllocator->GpuAllocate(mCpuHiZUavAllocInfo->NumDescriptors, mGpuHiZUavAllocInfo.get());

	uint32_t num = mCpuHiZUavAllocInfo->NumDescriptors;
	auto cpuHandle = mGlobalResources->CbvSrvUavAllocator->GetCpuHandle(mCpuHiZUavAllocInfo.get());
	auto shaderCpuHandle = mGlobalResources->CbvSrvUavAllocator->GetShaderCpuHandle(mGpuHiZUavAllocInfo.get());
	
	mGlobalResources->Device->CopyDescriptorsSimple(num, shaderCpuHandle, cpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}
