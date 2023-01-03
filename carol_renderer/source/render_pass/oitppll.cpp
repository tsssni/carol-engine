#include <render_pass/oitppll.h>
#include <render_pass/global_resources.h>
#include <render_pass/display.h>
#include <render_pass/frame.h>
#include <render_pass/taa.h>
#include <render_pass/mesh.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/descriptor_allocator.h>
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
	 mOutputFormat(outputFormat)
{
	InitShaders();
	InitPSOs();
	OnResize();
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
}

void Carol::OitppllPass::OnResize()
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

void Carol::OitppllPass::ReleaseIntermediateBuffers()
{
	if (mOitppllBuffer)
	{
		mOitppllBuffer->ReleaseIntermediateBuffer();
	}
}

uint32_t Carol::OitppllPass::GetPpllUavIdx()
{
	return mGpuCbvSrvUavAllocInfo->StartOffset + PPLL_UAV;
}

uint32_t Carol::OitppllPass::GetOffsetUavIdx()
{
	return mGpuCbvSrvUavAllocInfo->StartOffset + OFFSET_UAV;
}

uint32_t Carol::OitppllPass::GetCounterUavIdx()
{
	return mGpuCbvSrvUavAllocInfo->StartOffset + COUNTER_UAV;
}

uint32_t Carol::OitppllPass::GetPpllSrvIdx()
{
	return mGpuCbvSrvUavAllocInfo->StartOffset + PPLL_SRV;
}

uint32_t Carol::OitppllPass::GetOffsetSrvIdx()
{
	return mGpuCbvSrvUavAllocInfo->StartOffset + OFFSET_SRV;
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
}

void Carol::OitppllPass::InitPSOs()
{
	auto buildStaticOitppllPsoDesc = *mGlobalResources->BasePsoDesc;
	auto buildStaticOitppllAS = (*mGlobalResources->Shaders)[L"CullAS"].get();
	auto buildStaicOitppllMS = (*mGlobalResources->Shaders)[L"OpaqueStaticMS"].get();
	auto buildStaticOitppllPS = (*mGlobalResources->Shaders)[L"BuildStaticOitppllPS"].get();
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

	auto drawOitppllPsoDesc = *mGlobalResources->BasePsoDesc;
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
}

void Carol::OitppllPass::InitResources()
{
	uint32_t linkedListBufferSize = (*mGlobalResources->ClientWidth) * (*mGlobalResources->ClientHeight) * 16 * sizeof(OitppllNode);
	auto linkedListBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(linkedListBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	mOitppllBuffer = make_unique<DefaultResource>(&linkedListBufferDesc, mGlobalResources->DefaultBuffersHeap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	uint32_t startOffsetBufferSize = (*mGlobalResources->ClientWidth) * (*mGlobalResources->ClientHeight) * sizeof(uint32_t);
	auto startOffsetBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(startOffsetBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	mStartOffsetBuffer = make_unique<DefaultResource>(&startOffsetBufferDesc, mGlobalResources->DefaultBuffersHeap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	
	uint32_t counterBufferSize = sizeof(uint32_t);
	auto counterBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(counterBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	mCounterBuffer = make_unique<DefaultResource>(&counterBufferDesc, mGlobalResources->DefaultBuffersHeap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	InitDescriptors();
}

void Carol::OitppllPass::InitDescriptors()
{
	mGlobalResources->CbvSrvUavAllocator->CpuAllocate(OITPPLL_CBV_SRV_UAV_COUNT, mCpuCbvSrvUavAllocInfo.get());

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.StructureByteStride = sizeof(OitppllNode);
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = (*mGlobalResources->ClientWidth) * (*mGlobalResources->ClientHeight) * 16;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;

	mGlobalResources->Device->CreateUnorderedAccessView(mOitppllBuffer->Get(), mCounterBuffer->Get(), &uavDesc, GetCpuCbvSrvUav(PPLL_UAV));

	uavDesc.Buffer.StructureByteStride = 0;
	uavDesc.Buffer.NumElements = (*mGlobalResources->ClientWidth) * (*mGlobalResources->ClientHeight);
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
	uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;

	mGlobalResources->Device->CreateUnorderedAccessView(mStartOffsetBuffer->Get(), nullptr, &uavDesc, GetCpuCbvSrvUav(OFFSET_UAV));

	uavDesc.Buffer.NumElements = 1;
	mGlobalResources->Device->CreateUnorderedAccessView(mCounterBuffer->Get(), nullptr, &uavDesc, GetCpuCbvSrvUav(COUNTER_UAV));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.StructureByteStride = sizeof(OitppllNode);
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = (*mGlobalResources->ClientWidth) * (*mGlobalResources->ClientHeight) * 16;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;

	mGlobalResources->Device->CreateShaderResourceView(mOitppllBuffer->Get(), &srvDesc, GetCpuCbvSrvUav(PPLL_SRV));

	srvDesc.Buffer.StructureByteStride = 0;
	srvDesc.Buffer.NumElements = (*mGlobalResources->ClientWidth) * (*mGlobalResources->ClientHeight);
	srvDesc.Format = DXGI_FORMAT_R32_UINT;
	mGlobalResources->Device->CreateShaderResourceView(mStartOffsetBuffer->Get(), &srvDesc, GetCpuCbvSrvUav(OFFSET_SRV));

	CopyDescriptors();
}

void Carol::OitppllPass::DrawPpll()
{
	mGlobalResources->CommandList->RSSetViewports(1, mGlobalResources->ScreenViewport);
	mGlobalResources->CommandList->RSSetScissorRects(1, mGlobalResources->ScissorRect);

	static const uint32_t initOffsetValue = UINT32_MAX;
	static const uint32_t initCounterValue = 0;
	mGlobalResources->CommandList->ClearUnorderedAccessViewUint(GetGpuCbvSrvUav(OFFSET_UAV), GetCpuCbvSrvUav(OFFSET_UAV), mStartOffsetBuffer->Get(), &initOffsetValue, 0, nullptr);
	mGlobalResources->CommandList->ClearUnorderedAccessViewUint(GetGpuCbvSrvUav(COUNTER_UAV), GetCpuCbvSrvUav(COUNTER_UAV), mCounterBuffer->Get(), &initCounterValue, 0, nullptr);

	mGlobalResources->Meshes->DrawMeshes({
		nullptr,
		nullptr,
		(*mGlobalResources->PSOs)[L"BuildStaticOitppll"].Get(),
		(*mGlobalResources->PSOs)[L"BuildSkinnedOitppll"].Get() });
}

void Carol::OitppllPass::DrawOit()
{
	mGlobalResources->CommandList->OMSetRenderTargets(1, GetRvaluePtr(mGlobalResources->Frame->GetFrameRtv()), true, nullptr);
	mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"DrawOitppll"].Get());
	mGlobalResources->CommandList->DispatchMesh(1, 1, 1);
}
