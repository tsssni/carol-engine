#include <render_pass/oitppll.h>
#include <render_pass/global_resources.h>
#include <render_pass/display.h>
#include <render_pass/frame.h>
#include <render_pass/taa.h>
#include <render_pass/mesh.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/descriptor.h>
#include <dx12/root_signature.h>
#include <dx12/shader.h>
#include <scene/scene.h>

namespace Carol {
	using std::vector;
	using std::wstring;
	using std::make_unique;
}

Carol::OitppllPass::OitppllPass(GlobalResources* globalResources, DXGI_FORMAT outputFormat)
	:RenderPass(globalResources),
	 mOutputFormat(outputFormat),
	mCulledCommandBuffer(globalResources->NumFrame),
	mCullIdx(TRANSPARENT_MESH_TYPE_COUNT)
{
	InitShaders();
	InitPSOs();

	for (int i = 0; i < mGlobalResources->NumFrame; ++i)
	{
		mCulledCommandBuffer[i].resize(TRANSPARENT_MESH_TYPE_COUNT);

		for (int j = 0; j < TRANSPARENT_MESH_TYPE_COUNT; ++j)
		{
			ResizeCommandBuffer(mCulledCommandBuffer[i][j], 1024, sizeof(IndirectCommand));
		}
	}

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		mCullIdx[i].resize(CULL_IDX_COUNT);
	}
}

void Carol::OitppllPass::Cull()
{
	if (mGlobalResources->Scene->IsAnyTransparentMeshes())
	{
		uint32_t currFrame = *mGlobalResources->CurrFrame;
		mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"OitppllCull"].Get());

		for (int i = 0; i < TRANSPARENT_MESH_TYPE_COUNT; ++i)
		{
			if (mCullIdx[i][CULL_MESH_COUNT] == 0)
			{
				continue;
			}

			mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[currFrame][i]->Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST)));
			mCulledCommandBuffer[currFrame][i]->ResetCounter(mGlobalResources->CommandList);
			mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[currFrame][i]->Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)));
		
			uint32_t count = ceilf(mCullIdx[i][CULL_MESH_COUNT] / 32.f);

			mGlobalResources->CommandList->SetComputeRoot32BitConstants(PASS_CONSTANTS, CULL_IDX_COUNT, mCullIdx[i].data(), 0);
			mGlobalResources->CommandList->Dispatch(count, 1, 1);
			mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[currFrame][i]->Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)));
		}
	}
}

void Carol::OitppllPass::Draw()
{
	if (mGlobalResources->Scene->IsAnyTransparentMeshes())
	{
		DrawPpll();
		DrawOit();
	}
}

void Carol::OitppllPass::Update()
{
	uint32_t currFrame = *mGlobalResources->CurrFrame;

	for (int i = 0; i < TRANSPARENT_MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(TRANSPARENT_MESH_START + i);
		TestCommandBufferSize(mCulledCommandBuffer[currFrame][i], mGlobalResources->Scene->GetMeshesCount(type));

		mCullIdx[i][CULL_CULLED_COMMAND_BUFFER_IDX] = mCulledCommandBuffer[currFrame][i]->GetGpuUavIdx();
		mCullIdx[i][CULL_MESH_COUNT] = mGlobalResources->Scene->GetMeshesCount(type);
		mCullIdx[i][CULL_MESH_OFFSET] = mGlobalResources->Meshes->GetMeshCBStartOffet(type);
		mCullIdx[i][CULL_HIST] = 0;
	}
}

void Carol::OitppllPass::OnResize()
{
	static uint32_t width = 0;
    static uint32_t height = 0;

    if (width != *mGlobalResources->ClientWidth || height != *mGlobalResources->ClientHeight)
    {
        width = *mGlobalResources->ClientWidth;
        height = *mGlobalResources->ClientHeight;

        InitBuffers();
    }
}

void Carol::OitppllPass::ReleaseIntermediateBuffers()
{
}

uint32_t Carol::OitppllPass::GetPpllUavIdx()
{
	return mOitppllBuffer->GetGpuUavIdx();
}

uint32_t Carol::OitppllPass::GetOffsetUavIdx()
{
	return mStartOffsetBuffer->GetGpuUavIdx();
}

uint32_t Carol::OitppllPass::GetCounterUavIdx()
{
	return mCounterBuffer->GetGpuUavIdx();
}

uint32_t Carol::OitppllPass::GetPpllSrvIdx()
{
	return mOitppllBuffer->GetGpuSrvIdx();
}

uint32_t Carol::OitppllPass::GetOffsetSrvIdx()
{
	return mStartOffsetBuffer->GetGpuSrvIdx();
}

void Carol::OitppllPass::InitShaders()
{
	vector<wstring> nullDefines{};

	vector<wstring> staticDefines =
	{
		L"TAA=1",L"SSAO=1"
	};

	vector<wstring> skinnedDefines =
	{
		L"TAA=1",L"SSAO=1",L"SKINNED=1"
	};

	(*mGlobalResources->Shaders)[L"BuildStaticOitppllPS"] = make_unique<Shader>(L"shader\\oitppll_build_ps.hlsl", staticDefines, L"main", L"ps_6_6");
	(*mGlobalResources->Shaders)[L"BuildSkinnedOitppllPS"] = make_unique<Shader>(L"shader\\oitppll_build_ps.hlsl", skinnedDefines, L"main", L"ps_6_6");
	(*mGlobalResources->Shaders)[L"DrawOitppllPS"] = make_unique<Shader>(L"shader\\oitppll_ps.hlsl", nullDefines, L"main", L"ps_6_6");
	(*mGlobalResources->Shaders)[L"TransparentCS"] = make_unique<Shader>(L"shader\\cull_cs.hlsl", nullDefines, L"main", L"cs_6_6");
	(*mGlobalResources->Shaders)[L"TransparentAS"] = make_unique<Shader>(L"shader\\cull_as.hlsl", nullDefines, L"main", L"as_6_6");
}

void Carol::OitppllPass::InitPSOs()
{
	auto buildStaticOitppllPsoDesc = *mGlobalResources->BaseGraphicsPsoDesc;
	auto buildStaticOitppllAS = (*mGlobalResources->Shaders)[L"TransparentAS"].get();
	auto buildStaicOitppllMS = (*mGlobalResources->Shaders)[L"OpaqueStaticMS"].get();
	auto buildStaticOitppllPS = (*mGlobalResources->Shaders)[L"BuildStaticOitppllPS"].get();
	buildStaticOitppllPsoDesc.AS = { reinterpret_cast<byte*>(buildStaticOitppllAS->GetBufferPointer()), buildStaticOitppllAS->GetBufferSize() };
	buildStaticOitppllPsoDesc.MS = { reinterpret_cast<byte*>(buildStaicOitppllMS->GetBufferPointer()),buildStaicOitppllMS->GetBufferSize() };
	buildStaticOitppllPsoDesc.PS = { reinterpret_cast<byte*>(buildStaticOitppllPS->GetBufferPointer()),buildStaticOitppllPS->GetBufferSize() };
	buildStaticOitppllPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	buildStaticOitppllPsoDesc.DepthStencilState.DepthEnable = false;
	buildStaticOitppllPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	buildStaticOitppllPsoDesc.NumRenderTargets = 0;
	buildStaticOitppllPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	buildStaticOitppllPsoDesc.DepthStencilState.DepthEnable = false;
	buildStaticOitppllPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	auto buildStaticOitppllPsoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(buildStaticOitppllPsoDesc);
    D3D12_PIPELINE_STATE_STREAM_DESC buildStaticOitppllStreamDesc;
    buildStaticOitppllStreamDesc.pPipelineStateSubobjectStream = &buildStaticOitppllPsoStream;
    buildStaticOitppllStreamDesc.SizeInBytes = sizeof(buildStaticOitppllPsoStream);
    ThrowIfFailed(mGlobalResources->Device->CreatePipelineState(&buildStaticOitppllStreamDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"BuildStaticOitppll"].GetAddressOf())));

	auto buildSkinnedOitppllPsoDesc = buildStaticOitppllPsoDesc;
	auto buildSkinnedOitppllMS = (*mGlobalResources->Shaders)[L"OpaqueSkinnedMS"].get();
	auto buildSkinnedOitppllPS = (*mGlobalResources->Shaders)[L"BuildSkinnedOitppllPS"].get();
	buildSkinnedOitppllPsoDesc.MS = { reinterpret_cast<byte*>(buildSkinnedOitppllMS->GetBufferPointer()),buildSkinnedOitppllMS->GetBufferSize() };
	buildSkinnedOitppllPsoDesc.PS = { reinterpret_cast<byte*>(buildSkinnedOitppllPS->GetBufferPointer()),buildSkinnedOitppllPS->GetBufferSize() };
	auto buildSkinnedOitppllPsoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(buildSkinnedOitppllPsoDesc);
    D3D12_PIPELINE_STATE_STREAM_DESC buildSkinnedOitppllStreamDesc;
    buildSkinnedOitppllStreamDesc.pPipelineStateSubobjectStream = &buildSkinnedOitppllPsoStream;
    buildSkinnedOitppllStreamDesc.SizeInBytes = sizeof(buildSkinnedOitppllPsoStream);
    ThrowIfFailed(mGlobalResources->Device->CreatePipelineState(&buildSkinnedOitppllStreamDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"BuildSkinnedOitppll"].GetAddressOf())));

	auto drawOitppllPsoDesc = *mGlobalResources->BaseGraphicsPsoDesc;
	auto drawStaicOitppllMS = (*mGlobalResources->Shaders)[L"ScreenMS"].get();
	auto drawOitppllPS = (*mGlobalResources->Shaders)[L"DrawOitppllPS"].get();
	drawOitppllPsoDesc.MS = { reinterpret_cast<byte*>(drawStaicOitppllMS->GetBufferPointer()),drawStaicOitppllMS->GetBufferSize() };
	drawOitppllPsoDesc.PS = { reinterpret_cast<byte*>(drawOitppllPS->GetBufferPointer()),drawOitppllPS->GetBufferSize() };
	drawOitppllPsoDesc.BlendState.RenderTarget[0].BlendEnable = true;
	drawOitppllPsoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	drawOitppllPsoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	drawOitppllPsoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	drawOitppllPsoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	drawOitppllPsoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
	drawOitppllPsoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	drawOitppllPsoDesc.BlendState.RenderTarget[0].LogicOpEnable = false;
	drawOitppllPsoDesc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	drawOitppllPsoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	drawOitppllPsoDesc.DepthStencilState.DepthEnable = false;
	drawOitppllPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	auto drawOitppllPsoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(drawOitppllPsoDesc);
    D3D12_PIPELINE_STATE_STREAM_DESC drawOitppllStreamDesc;
    drawOitppllStreamDesc.pPipelineStateSubobjectStream = &drawOitppllPsoStream;
    drawOitppllStreamDesc.SizeInBytes = sizeof(drawOitppllPsoStream);
    ThrowIfFailed(mGlobalResources->Device->CreatePipelineState(&drawOitppllStreamDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"DrawOitppll"].GetAddressOf())));

	auto oitppllCullPsoDesc = *mGlobalResources->BaseComputePsoDesc;
	auto oitppllCullCS = (*mGlobalResources->Shaders)[L"TransparentCS"].get();
	oitppllCullPsoDesc.CS = { reinterpret_cast<byte*>(oitppllCullCS->GetBufferPointer()), oitppllCullCS->GetBufferSize() };
	ThrowIfFailed(mGlobalResources->Device->CreateComputePipelineState(&oitppllCullPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"OitppllCull"].GetAddressOf())));
}

void Carol::OitppllPass::InitBuffers()
{
	uint32_t width = *mGlobalResources->ClientWidth;
	uint32_t height = *mGlobalResources->ClientHeight;

	mOitppllBuffer = make_unique<StructuredBuffer>(
		width * height * 2,
		sizeof(OitppllNode),
		mGlobalResources->HeapManager->GetDefaultBuffersHeap(),
		mGlobalResources->DescriptorManager,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mStartOffsetBuffer = make_unique<RawBuffer>(
		width * height * sizeof(uint32_t),
		mGlobalResources->HeapManager->GetDefaultBuffersHeap(),
		mGlobalResources->DescriptorManager,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	
	mCounterBuffer = make_unique<RawBuffer>(
		sizeof(uint32_t),
		mGlobalResources->HeapManager->GetDefaultBuffersHeap(),
		mGlobalResources->DescriptorManager,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}

void Carol::OitppllPass::TestCommandBufferSize(std::unique_ptr<StructuredBuffer>& buffer, uint32_t numElements)
{
	if (buffer->GetNumElements() < numElements)
	{
		ResizeCommandBuffer(buffer, numElements, buffer->GetElementSize());
	}
}

void Carol::OitppllPass::ResizeCommandBuffer(std::unique_ptr<StructuredBuffer>& buffer, uint32_t numElements, uint32_t elementSize)
{
	buffer = make_unique<StructuredBuffer>(
		numElements,
		elementSize,
		mGlobalResources->HeapManager->GetDefaultBuffersHeap(),
		mGlobalResources->DescriptorManager,
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}

void Carol::OitppllPass::DrawPpll()
{
	static const uint32_t initOffsetValue = UINT32_MAX;
	static const uint32_t initCounterValue = 0;
	uint32_t currFrame = *mGlobalResources->CurrFrame;

	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mOitppllBuffer->Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)));
	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCounterBuffer->Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)));

	mGlobalResources->CommandList->ClearUnorderedAccessViewUint(mStartOffsetBuffer->GetGpuUav(), mStartOffsetBuffer->GetCpuUav(), mStartOffsetBuffer->Get(), &initOffsetValue, 0, nullptr);
	mGlobalResources->CommandList->ClearUnorderedAccessViewUint(mCounterBuffer->GetGpuUav(), mCounterBuffer->GetCpuUav(), mCounterBuffer->Get(), &initCounterValue, 0, nullptr);
	
	mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"BuildStaticOitppll"].Get());
	mGlobalResources->Meshes->ExecuteIndirect(mCulledCommandBuffer[currFrame][TRANSPARENT_STATIC - TRANSPARENT_MESH_START].get());
	
	mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"BuildSkinnedOitppll"].Get());
	mGlobalResources->Meshes->ExecuteIndirect(mCulledCommandBuffer[currFrame][TRANSPARENT_SKINNED - TRANSPARENT_MESH_START].get());
}

void Carol::OitppllPass::DrawOit()
{
	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mOitppllBuffer->Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));
	mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCounterBuffer->Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));

	mGlobalResources->CommandList->OMSetRenderTargets(1, GetRvaluePtr(mGlobalResources->Frame->GetFrameRtv()), true, nullptr);
	mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"DrawOitppll"].Get());
	mGlobalResources->CommandList->DispatchMesh(1, 1, 1);
}
