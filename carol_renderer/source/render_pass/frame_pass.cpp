#include <render_pass/frame_pass.h>
#include <scene/scene.h>
#include <dx12.h>
#include <global.h>
#include <DirectXColors.h>
#include <string_view>
#include <span>

namespace Carol
{
	using std::vector;
	using std::wstring;
	using std::wstring_view;
	using std::unique_ptr;
	using std::make_unique;
	using std::span;
	using Microsoft::WRL::ComPtr;
	using namespace DirectX;
}

Carol::FramePass::FramePass(
	ID3D12Device* device,
	Heap* heap,
	DescriptorManager* descriptorManager,
	Scene* scene,
	DXGI_FORMAT frameFormat,
	DXGI_FORMAT depthStencilFormat,
	DXGI_FORMAT hiZFormat)
	:mScene(scene),
	mCulledCommandBuffer(MESH_TYPE_COUNT),
	mCullIdx(MESH_TYPE_COUNT),
	mHiZIdx(HIZ_IDX_COUNT),
	mFrameFormat(frameFormat),
	mDepthStencilFormat(depthStencilFormat),
	mHiZFormat(hiZFormat)
{
	InitShaders();
	InitPSOs(device);

	mCulledCommandBufferPool = make_unique<StructuredBufferPool>(
		1024,
		sizeof(IndirectCommand),
		device,
		heap,
		descriptorManager,
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(OPAQUE_MESH_START + i);
		mCullIdx[type].resize(CULL_IDX_COUNT);
	}
}

void Carol::FramePass::Draw(ID3D12GraphicsCommandList* cmdList)
{
	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	mFrameMap->Transition(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	cmdList->ClearRenderTargetView(mFrameMap->GetRtv(), Colors::Gray, 0, nullptr);
	cmdList->ClearDepthStencilView(mDepthStencilMap->GetDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);

	DrawOpaque(cmdList);
	DrawTransparent(cmdList);
}

void Carol::FramePass::Update(uint64_t cpuFenceValue, uint64_t completedFenceValue)
{
	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(i);

		mCulledCommandBufferPool->DiscardBuffer(mCulledCommandBuffer[type].release(), cpuFenceValue);
		mCulledCommandBuffer[type] = mCulledCommandBufferPool->RequestBuffer(completedFenceValue, mScene->GetMeshesCount(type));

		mCullIdx[type][CULL_CULLED_COMMAND_BUFFER_IDX] = mCulledCommandBuffer[type]->GetGpuUavIdx();
		mCullIdx[type][CULL_MESH_COUNT] = mScene->GetMeshesCount(type);
		mCullIdx[type][CULL_MESH_OFFSET] = mScene->GetMeshCBStartOffet(type);
		mCullIdx[type][CULL_HIZ_IDX] = mHiZMap->GetGpuSrvIdx();
	}

	mHiZIdx[HIZ_DEPTH_IDX] = mDepthStencilMap->GetGpuSrvIdx();
	mHiZIdx[HIZ_IDX] = mHiZMap->GetGpuSrvIdx();
	mHiZIdx[RW_HIZ_IDX] = mHiZMap->GetGpuUavIdx();
}

void Carol::FramePass::Cull(ID3D12GraphicsCommandList* cmdList)
{
	Clear(cmdList);
	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	for (int i = 0; i < 2; ++i)
	{
		CullInstances(true, i, cmdList);
		CullMeshlets(true, i, cmdList);
		GenerateHiZ(cmdList);
	}
	
	CullInstances(false, 0, cmdList);
	CullMeshlets(false, 0, cmdList);
}

void Carol::FramePass::SetFrameMap(ColorBuffer* frameMap)
{
	mFrameMap = frameMap;
}

void Carol::FramePass::SetDepthStencilMap(ColorBuffer* depthStencilMap)
{
	mDepthStencilMap = depthStencilMap;
}

const Carol::StructuredBuffer* Carol::FramePass::GetIndirectCommandBuffer(MeshType type)const
{
	return mCulledCommandBuffer[type].get();
}

uint32_t Carol::FramePass::GetHiZSrvIdx()const
{
	return mHiZMap->GetGpuSrvIdx();
}

uint32_t Carol::FramePass::GetHiZUavIdx()const
{
	return mHiZMap->GetGpuUavIdx();
}

uint32_t Carol::FramePass::GetPpllUavIdx()const
{
	return mOitppllBuffer->GetGpuUavIdx();
}

uint32_t Carol::FramePass::GetOffsetUavIdx()const
{
	return mStartOffsetBuffer->GetGpuUavIdx();
}

uint32_t Carol::FramePass::GetCounterUavIdx()const
{
	return mCounterBuffer->GetGpuUavIdx();
}

uint32_t Carol::FramePass::GetPpllSrvIdx()const
{
	return mOitppllBuffer->GetGpuSrvIdx();
}

uint32_t Carol::FramePass::GetOffsetSrvIdx()const
{
	return mStartOffsetBuffer->GetGpuSrvIdx();
}

void Carol::FramePass::InitShaders()
{
	vector<wstring_view> nullDefines = {};

	vector<wstring_view> staticDefines =
	{
		L"TAA"
	};

	vector<wstring_view> skinnedDefines =
	{
		L"TAA", L"SKINNED" 
	};

	vector<wstring_view> blinnPhongDefines =
	{
		L"SSAO", L"BLINN_PHONG"
	};

	vector<wstring_view> pbrDefines =
	{
		L"SSAO",L"GGX",L"SMITH",L"HEIGHT_CORRELATED",L"LAMBERTIAN"
	};

	vector<wstring_view> cullDefines =
	{
		L"FRUSTUM", L"NORMAL_CONE", L"HIZ_OCCLUSION"
	};

	vector<wstring_view> cullWriteDefines =
	{
		L"FRUSTUM", L"NORMAL_CONE", L"HIZ_OCCLUSION", L"WRITE"
	};

	vector<wstring_view> transparentCullWriteDefines =
	{
		L"FRUSTUM", L"NORMAL_CONE", L"HIZ_OCCLUSION", L"WRITE", L"TRANSPARENT"
	};

	if (gShaders.count(L"MeshStaticMS") == 0)
	{
		gShaders[L"MeshStaticMS"] = make_unique<Shader>(L"shader\\mesh_ms.hlsl", staticDefines, L"main", L"ms_6_6");
	}
	
	if (gShaders.count(L"MeshSkinnedMS") == 0)
	{
		gShaders[L"MeshSkinnedMS"] = make_unique<Shader>(L"shader\\mesh_ms.hlsl", skinnedDefines, L"main", L"ms_6_6");
	}


	if (gShaders.count(L"BlinnPhongPS") == 0)
	{
		gShaders[L"BlinnPhongPS"] = make_unique<Shader>(L"shader\\opaque_ps.hlsl", blinnPhongDefines, L"main", L"ps_6_6");
	}

	if (gShaders.count(L"PBRPS") == 0)
	{
		gShaders[L"PBRPS"] = make_unique<Shader>(L"shader\\opaque_ps.hlsl", pbrDefines, L"main", L"ps_6_6");
	}

	if (gShaders.count(L"SkyBoxMS") == 0)
	{
		gShaders[L"SkyBoxMS"] = make_unique<Shader>(L"shader\\skybox_ms.hlsl", staticDefines, L"main", L"ms_6_6");
	}

	if (gShaders.count(L"SkyBoxPS") == 0)
	{
		gShaders[L"SkyBoxPS"] = make_unique<Shader>(L"shader\\skybox_ps.hlsl", staticDefines, L"main", L"ps_6_6");
	}

	if (gShaders.count(L"HiZGenerateCS") == 0)
	{
		gShaders[L"HiZGenerateCS"] = make_unique<Shader>(L"shader\\hiz_generate_cs.hlsl", nullDefines, L"main", L"cs_6_6");
	}

	if (gShaders.count(L"FrameCullCS") == 0)
	{
		gShaders[L"FrameCullCS"] = make_unique<Shader>(L"shader\\cull_cs.hlsl", cullWriteDefines, L"main", L"cs_6_6");
	}

	if (gShaders.count(L"CullAS") == 0)
	{
		gShaders[L"CullAS"] = make_unique<Shader>(L"shader\\cull_as.hlsl", cullDefines, L"main", L"as_6_6");
	}

	if (gShaders.count(L"CullWriteAS") == 0)
	{
		gShaders[L"CullWriteAS"] = make_unique<Shader>(L"shader\\cull_as.hlsl", cullWriteDefines, L"main", L"as_6_6");
	}

	if (gShaders.count(L"DepthStaticMS") == 0)
	{
		gShaders[L"DepthStaticMS"] = make_unique<Shader>(L"shader\\depth_ms.hlsl", nullDefines, L"main", L"ms_6_6");
	}

	if (gShaders.count(L"DepthSkinnedMS") == 0)
	{
		gShaders[L"DepthSkinnedMS"] = make_unique<Shader>(L"shader\\depth_ms.hlsl", skinnedDefines, L"main", L"ms_6_6");
	}

	if (gShaders.count(L"BlinnPhongOitppllPS") == 0)
	{
		gShaders[L"BlinnPhongOitppllPS"] = make_unique<Shader>(L"shader\\oitppll_build_ps.hlsl", blinnPhongDefines, L"main", L"ps_6_6");
	}

	if (gShaders.count(L"PBROitppllPS") == 0)
	{
		gShaders[L"PBROitppllPS"] = make_unique<Shader>(L"shader\\oitppll_build_ps.hlsl", pbrDefines, L"main", L"ps_6_6");
	}

	if (gShaders.count(L"DrawOitppllPS") == 0)
	{
		gShaders[L"DrawOitppllPS"] = make_unique<Shader>(L"shader\\oitppll_ps.hlsl", nullDefines, L"main", L"ps_6_6");
	}

	if (gShaders.count(L"TransparentCullWriteAS") == 0)
	{
		gShaders[L"TransparentCullWriteAS"] = make_unique<Shader>(L"shader\\cull_as.hlsl", transparentCullWriteDefines, L"main", L"as_6_6");
	}
}

void Carol::FramePass::InitPSOs(ID3D12Device* device)
{
	if (gPSOs.count(L"OpaqueStaticMeshletCull") == 0)
	{
		auto cullStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
		cullStaticMeshPSO->SetRootSignature(sRootSignature.get());
		cullStaticMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthStencilFormat));
		cullStaticMeshPSO->SetAS(gShaders[L"CullWriteAS"].get());
		cullStaticMeshPSO->SetMS(gShaders[L"DepthStaticMS"].get());
		cullStaticMeshPSO->Finalize(device);

		gPSOs[L"OpaqueStaticMeshletCull"] = std::move(cullStaticMeshPSO);
	}

	if (gPSOs.count(L"OpaqueSkinnedMeshletCull") == 0)
	{
		auto cullSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
		cullSkinnedMeshPSO->SetRootSignature(sRootSignature.get());
		cullSkinnedMeshPSO->SetDepthTargetFormat(GetDsvFormat(mDepthStencilFormat));
		cullSkinnedMeshPSO->SetAS(gShaders[L"CullWriteAS"].get());
		cullSkinnedMeshPSO->SetMS(gShaders[L"DepthSkinnedMS"].get());
		cullSkinnedMeshPSO->Finalize(device);

		gPSOs[L"OpaqueSkinnedMeshletCull"] = std::move(cullSkinnedMeshPSO);
	}

	if (gPSOs.count(L"BlinnPhongStatic") == 0)
	{
		auto blinnPhongStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
		blinnPhongStaticMeshPSO->SetRootSignature(sRootSignature.get());
		blinnPhongStaticMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
		blinnPhongStaticMeshPSO->SetAS(gShaders[L"CullAS"].get());
		blinnPhongStaticMeshPSO->SetMS(gShaders[L"MeshStaticMS"].get());
		blinnPhongStaticMeshPSO->SetPS(gShaders[L"BlinnPhongPS"].get());
		blinnPhongStaticMeshPSO->Finalize(device);

		gPSOs[L"BlinnPhongStatic"] = std::move(blinnPhongStaticMeshPSO);
	}

	if (gPSOs.count(L"BlinnPhongSkinned") == 0)
	{
		auto blinnPhongSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
		blinnPhongSkinnedMeshPSO->SetRootSignature(sRootSignature.get());
		blinnPhongSkinnedMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
		blinnPhongSkinnedMeshPSO->SetAS(gShaders[L"CullAS"].get());
		blinnPhongSkinnedMeshPSO->SetMS(gShaders[L"MeshSkinnedMS"].get());
		blinnPhongSkinnedMeshPSO->SetPS(gShaders[L"BlinnPhongPS"].get());
		blinnPhongSkinnedMeshPSO->Finalize(device);

		gPSOs[L"BlinnPhongSkinned"] = std::move(blinnPhongSkinnedMeshPSO);
	}

	if (gPSOs.count(L"PBRStatic") == 0)
	{
		auto pbrStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
		pbrStaticMeshPSO->SetRootSignature(sRootSignature.get());
		pbrStaticMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
		pbrStaticMeshPSO->SetAS(gShaders[L"CullAS"].get());
		pbrStaticMeshPSO->SetMS(gShaders[L"MeshStaticMS"].get());
		pbrStaticMeshPSO->SetPS(gShaders[L"PBRPS"].get());
		pbrStaticMeshPSO->Finalize(device);

		gPSOs[L"PBRStatic"] = std::move(pbrStaticMeshPSO);
	}

	if (gPSOs.count(L"PBRSkinned") == 0)
	{
		auto pbrSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
		pbrSkinnedMeshPSO->SetRootSignature(sRootSignature.get());
		pbrSkinnedMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
		pbrSkinnedMeshPSO->SetAS(gShaders[L"CullAS"].get());
		pbrSkinnedMeshPSO->SetMS(gShaders[L"MeshSkinnedMS"].get());
		pbrSkinnedMeshPSO->SetPS(gShaders[L"PBRPS"].get());
		pbrSkinnedMeshPSO->Finalize(device);

		gPSOs[L"PBRSkinned"] = std::move(pbrSkinnedMeshPSO);
	}

	if (gPSOs.count(L"SkyBox") == 0)
	{
		auto skyBoxMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
		skyBoxMeshPSO->SetRootSignature(sRootSignature.get());
		skyBoxMeshPSO->SetDepthStencilState(gDepthLessEqualState);
		skyBoxMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
		skyBoxMeshPSO->SetMS(gShaders[L"SkyBoxMS"].get());
		skyBoxMeshPSO->SetPS(gShaders[L"SkyBoxPS"].get());
		skyBoxMeshPSO->Finalize(device);

		gPSOs[L"SkyBox"] = std::move(skyBoxMeshPSO);
	}

	if (gPSOs.count(L"HiZGenerate") == 0)
	{
		auto hiZGenerateComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
		hiZGenerateComputePSO->SetRootSignature(sRootSignature.get());
		hiZGenerateComputePSO->SetCS(gShaders[L"HiZGenerateCS"].get());
		hiZGenerateComputePSO->Finalize(device);

		gPSOs[L"HiZGenerate"] = std::move(hiZGenerateComputePSO);
	}

	if (gPSOs.count(L"FrameCullInstances") == 0)
	{
		auto cullInstanceComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
		cullInstanceComputePSO->SetRootSignature(sRootSignature.get());
		cullInstanceComputePSO->SetCS(gShaders[L"FrameCullCS"].get());
		cullInstanceComputePSO->Finalize(device);

		gPSOs[L"FrameCullInstances"] = std::move(cullInstanceComputePSO);
	}

	if (gPSOs.count(L"BlinnPhongStaticOitppll") == 0)
	{
		auto blinnPhongStaticOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
		blinnPhongStaticOitppllMeshPSO->SetRootSignature(sRootSignature.get());
		blinnPhongStaticOitppllMeshPSO->SetRasterizerState(gCullDisabledState);
		blinnPhongStaticOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState);
		blinnPhongStaticOitppllMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
		blinnPhongStaticOitppllMeshPSO->SetAS(gShaders[L"CullAS"].get());
		blinnPhongStaticOitppllMeshPSO->SetMS(gShaders[L"MeshStaticMS"].get());
		blinnPhongStaticOitppllMeshPSO->SetPS(gShaders[L"BlinnPhongOitppllPS"].get());
		blinnPhongStaticOitppllMeshPSO->Finalize(device);

		gPSOs[L"BlinnPhongStaticOitppll"] = std::move(blinnPhongStaticOitppllMeshPSO);
	}

	if (gPSOs.count(L"BlinnPhongSkinnedOitppll") == 0)
	{
		auto blinnPhongSkinnedOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
		blinnPhongSkinnedOitppllMeshPSO->SetRootSignature(sRootSignature.get());
		blinnPhongSkinnedOitppllMeshPSO->SetRasterizerState(gCullDisabledState);
		blinnPhongSkinnedOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState);
		blinnPhongSkinnedOitppllMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
		blinnPhongSkinnedOitppllMeshPSO->SetAS(gShaders[L"CullAS"].get());
		blinnPhongSkinnedOitppllMeshPSO->SetMS(gShaders[L"MeshSkinnedMS"].get());
		blinnPhongSkinnedOitppllMeshPSO->SetPS(gShaders[L"BlinnPhongOitppllPS"].get());
		blinnPhongSkinnedOitppllMeshPSO->Finalize(device);

		gPSOs[L"BlinnPhongSkinnedOitppll"] = std::move(blinnPhongSkinnedOitppllMeshPSO);
	}

	if (gPSOs.count(L"PBRStaticOitppll") == 0)
	{
		auto pbrStaticOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
		pbrStaticOitppllMeshPSO->SetRootSignature(sRootSignature.get());
		pbrStaticOitppllMeshPSO->SetRasterizerState(gCullDisabledState);
		pbrStaticOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState);
		pbrStaticOitppllMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
		pbrStaticOitppllMeshPSO->SetAS(gShaders[L"CullAS"].get());
		pbrStaticOitppllMeshPSO->SetMS(gShaders[L"MeshStaticMS"].get());
		pbrStaticOitppllMeshPSO->SetPS(gShaders[L"PBROitppllPS"].get());
		pbrStaticOitppllMeshPSO->Finalize(device);

		gPSOs[L"PBRStaticOitppll"] = std::move(pbrStaticOitppllMeshPSO);
	}

	if (gPSOs.count(L"PBRSkinnedOitppll") == 0)
	{
		auto pbrSkinnedOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
		pbrSkinnedOitppllMeshPSO->SetRootSignature(sRootSignature.get());
		pbrSkinnedOitppllMeshPSO->SetRasterizerState(gCullDisabledState);
		pbrSkinnedOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState);
		pbrSkinnedOitppllMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
		pbrSkinnedOitppllMeshPSO->SetAS(gShaders[L"CullAS"].get());
		pbrSkinnedOitppllMeshPSO->SetMS(gShaders[L"MeshSkinnedMS"].get());
		pbrSkinnedOitppllMeshPSO->SetPS(gShaders[L"PBROitppllPS"].get());
		pbrSkinnedOitppllMeshPSO->Finalize(device);

		gPSOs[L"PBRSkinnedOitppll"] = std::move(pbrSkinnedOitppllMeshPSO);
	}

	if (gPSOs.count(L"DrawOitppll") == 0)
	{
		auto drawOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
		drawOitppllMeshPSO->SetRootSignature(sRootSignature.get());
		drawOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState);
		drawOitppllMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
		drawOitppllMeshPSO->SetBlendState(gAlphaBlendState);
		drawOitppllMeshPSO->SetRenderTargetFormat(mFrameFormat);
		drawOitppllMeshPSO->SetMS(gShaders[L"ScreenMS"].get());
		drawOitppllMeshPSO->SetPS(gShaders[L"DrawOitppllPS"].get());
		drawOitppllMeshPSO->Finalize(device);

		gPSOs[L"DrawOitppll"] = std::move(drawOitppllMeshPSO);
	}

	if (gPSOs.count(L"TransparentMeshletCull") == 0)
	{
		auto oitppllMeshletCullMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
		oitppllMeshletCullMeshPSO->SetRootSignature(sRootSignature.get());
		oitppllMeshletCullMeshPSO->SetRasterizerState(gCullDisabledState);
		oitppllMeshletCullMeshPSO->SetDepthStencilState(gDepthDisabledState);
		oitppllMeshletCullMeshPSO->SetAS(gShaders[L"TransparentCullWriteAS"].get());
		oitppllMeshletCullMeshPSO->SetMS(gShaders[L"DepthStaticMS"].get());
		oitppllMeshletCullMeshPSO->Finalize(device);

		gPSOs[L"TransparentMeshletCull"] = std::move(oitppllMeshletCullMeshPSO);
	}
}

void Carol::FramePass::InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)
{
	mHiZMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mHiZFormat,
		device,
		heap,
		descriptorManager,
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		nullptr,
		mMipLevel);

	mOitppllBuffer = make_unique<StructuredBuffer>(
		mWidth * mHeight * 16,
		sizeof(OitppllNode),
		device,
		heap,
		descriptorManager,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mStartOffsetBuffer = make_unique<RawBuffer>(
		mWidth * mHeight * sizeof(uint32_t),
		device,
		heap,
		descriptorManager,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mCounterBuffer = make_unique<RawBuffer>(
		sizeof(uint32_t),
		device,
		heap,
		descriptorManager,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}

void Carol::FramePass::DrawOpaque(ID3D12GraphicsCommandList* cmdList)
{
	cmdList->OMSetRenderTargets(1, GetRvaluePtr(mFrameMap->GetRtv()), true, GetRvaluePtr(mDepthStencilMap->GetDsv()));

	cmdList->SetPipelineState(gPSOs[L"PBRStatic"]->Get());
	ExecuteIndirect(cmdList, GetIndirectCommandBuffer(OPAQUE_STATIC));

	cmdList->SetPipelineState(gPSOs[L"PBRSkinned"]->Get());
	ExecuteIndirect(cmdList, GetIndirectCommandBuffer(OPAQUE_SKINNED));

	DrawSkyBox(cmdList, mScene->GetSkyBox()->GetMeshCBAddress());
}

void Carol::FramePass::DrawSkyBox(ID3D12GraphicsCommandList* cmdList, D3D12_GPU_VIRTUAL_ADDRESS skyBoxMeshCBAddr)
{
	cmdList->SetPipelineState(gPSOs[L"SkyBox"]->Get());
	cmdList->SetGraphicsRootConstantBufferView(MESH_CB, skyBoxMeshCBAddr);
	static_cast<ID3D12GraphicsCommandList6*>(cmdList)->DispatchMesh(1, 1, 1);
}

void Carol::FramePass::DrawTransparent(ID3D12GraphicsCommandList* cmdList)
{
	if (!mScene->IsAnyTransparentMeshes())
	{
		return;
	}

	constexpr uint32_t initOffsetValue = UINT32_MAX;
	constexpr uint32_t initCounterValue = 0;

	mOitppllBuffer->Transition(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	mStartOffsetBuffer->Transition(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	cmdList->ClearUnorderedAccessViewUint(mStartOffsetBuffer->GetGpuUav(), mStartOffsetBuffer->GetCpuUav(), mStartOffsetBuffer->Get(), &initOffsetValue, 0, nullptr);
	cmdList->ClearUnorderedAccessViewUint(mCounterBuffer->GetGpuUav(), mCounterBuffer->GetCpuUav(), mCounterBuffer->Get(), &initCounterValue, 0, nullptr);
	cmdList->OMSetRenderTargets(0, nullptr, true, nullptr);

	cmdList->SetPipelineState(gPSOs[L"PBRStaticOitppll"]->Get());
	ExecuteIndirect(cmdList, GetIndirectCommandBuffer(TRANSPARENT_STATIC));

	cmdList->SetPipelineState(gPSOs[L"PBRSkinnedOitppll"]->Get());
	ExecuteIndirect(cmdList, GetIndirectCommandBuffer(TRANSPARENT_SKINNED));

	mOitppllBuffer->Transition(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mStartOffsetBuffer->Transition(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	cmdList->OMSetRenderTargets(1, GetRvaluePtr(mFrameMap->GetRtv()), true, nullptr);
	cmdList->SetPipelineState(gPSOs[L"DrawOitppll"]->Get());
	static_cast<ID3D12GraphicsCommandList6*>(cmdList)->DispatchMesh(1, 1, 1);
}

void Carol::FramePass::Clear(ID3D12GraphicsCommandList* cmdList)
{
	mScene->ClearCullMark(cmdList);

	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(i);

		mCulledCommandBuffer[type]->Transition(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);
		mCulledCommandBuffer[type]->ResetCounter(cmdList);
	}
}

void Carol::FramePass::CullInstances(bool opaque, uint32_t iteration, ID3D12GraphicsCommandList* cmdList)
{
	cmdList->SetPipelineState(gPSOs[L"FrameCullInstances"]->Get());

	uint32_t start = opaque ? OPAQUE_MESH_START : TRANSPARENT_MESH_START;
	uint32_t end = start + (opaque ? OPAQUE_MESH_TYPE_COUNT : TRANSPARENT_MESH_TYPE_COUNT);

	for (int i = start; i < end; ++i)
	{
		MeshType type = MeshType(i);

		if (mCullIdx[type][CULL_MESH_COUNT] == 0)
		{
			continue;
		}

		mCullIdx[type][CULL_ITERATION] = iteration;
		uint32_t count = ceilf(mCullIdx[type][CULL_MESH_COUNT] / 32.f);

		mCulledCommandBuffer[type]->Transition(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		cmdList->SetComputeRoot32BitConstants(PASS_CONSTANTS, CULL_IDX_COUNT, mCullIdx[type].data(), 0);
		cmdList->Dispatch(count, 1, 1);
		mCulledCommandBuffer[type]->UavBarrier(cmdList);
	}
}

void Carol::FramePass::CullMeshlets(bool opaque, uint32_t iteration, ID3D12GraphicsCommandList* cmdList)
{
	ID3D12PipelineState* pso[] = {
		gPSOs[L"OpaqueStaticMeshletCull"]->Get(),
		gPSOs[L"OpaqueSkinnedMeshletCull"]->Get(),
		gPSOs[L"TransparentMeshletCull"]->Get(),
		gPSOs[L"TransparentMeshletCull"]->Get() };

	uint32_t start = opaque ? OPAQUE_MESH_START : TRANSPARENT_MESH_START;
	uint32_t end = start + (opaque ? OPAQUE_MESH_TYPE_COUNT : TRANSPARENT_MESH_TYPE_COUNT);

	if (opaque && iteration == 0)
	{
		cmdList->ClearDepthStencilView(mDepthStencilMap->GetDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
		cmdList->OMSetRenderTargets(0, nullptr, true, GetRvaluePtr(mDepthStencilMap->GetDsv()));
	}

	for (int i = start; i < end; ++i)
	{
		MeshType type = MeshType(i);
		mCulledCommandBuffer[type]->Transition(cmdList, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

		if (mCullIdx[type][CULL_MESH_COUNT] == 0)
		{
			continue;
		}

		mCullIdx[type][CULL_ITERATION] = iteration;

		cmdList->SetPipelineState(pso[i]);
		cmdList->SetGraphicsRoot32BitConstants(PASS_CONSTANTS, CULL_IDX_COUNT, mCullIdx[type].data(), 0);
		ExecuteIndirect(cmdList, mCulledCommandBuffer[type].get());
	}
}

void Carol::FramePass::GenerateHiZ(ID3D12GraphicsCommandList* cmdList)
{
	cmdList->SetPipelineState(gPSOs[L"HiZGenerate"]->Get());

	for (int i = 0; i < mMipLevel - 1; i += 5)
	{
		mHiZIdx[HIZ_SRC_MIP] = i;
		mHiZIdx[HIZ_NUM_MIP_LEVEL] = i + 5 >= mMipLevel ? mMipLevel - i - 1 : 5;
		cmdList->SetComputeRoot32BitConstants(PASS_CONSTANTS, mHiZIdx.size(), mHiZIdx.data(), 0);

		uint32_t width = ceilf((mWidth >> i) / 32.f);
		uint32_t height = ceilf((mHeight >> i) / 32.f);
		cmdList->Dispatch(width, height, 1);
		mHiZMap->UavBarrier(cmdList);
	}
}