#include <manager/oitppll.h>
#include <global_resources.h>
#include <manager/display.h>
#include <manager/taa.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/descriptor_allocator.h>
#include <dx12/shader.h>
#include <dx12/sampler.h>

namespace Carol {
	using std::vector;
	using std::wstring;
	using std::make_unique;
}

Carol::OitppllManager::OitppllManager(GlobalResources* globalResources, DXGI_FORMAT outputFormat)
	:Manager(globalResources), mOutputFormat(outputFormat)
{
	InitRootSignature();
	InitShaders();
	InitPSOs();
	OnResize();
}

void Carol::OitppllManager::Draw()
{
	if (mGlobalResources->TransparentStaticMeshes->size() == 0 
		&& mGlobalResources->TransparentSkinnedMeshes->size() == 0)
	{
		return;
	}

	DrawPpll();
	DrawOit();
}

void Carol::OitppllManager::Update()
{
}

void Carol::OitppllManager::OnResize()
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

void Carol::OitppllManager::ReleaseIntermediateBuffers()
{
	if (mOitppllBuffer)
	{
		mOitppllBuffer->ReleaseIntermediateBuffer();
	}
}

void Carol::OitppllManager::InitRootSignature()
{
}

void Carol::OitppllManager::InitShaders()
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

	(*mGlobalResources->Shaders)[L"BuildStaticOitppllVS"] = make_unique<Shader>(L"shader\\oitppll_build.hlsl", staticDefines, L"VS", L"vs_6_5");
	(*mGlobalResources->Shaders)[L"BuildStaticOitppllPS"] = make_unique<Shader>(L"shader\\oitppll_build.hlsl", staticDefines, L"PS", L"ps_6_5");
	(*mGlobalResources->Shaders)[L"BuildSkinnedOitppllVS"] = make_unique<Shader>(L"shader\\oitppll_build.hlsl", skinnedDefines, L"VS", L"vs_6_5");
	(*mGlobalResources->Shaders)[L"BuildSkinnedOitppllPS"] = make_unique<Shader>(L"shader\\oitppll_build.hlsl", skinnedDefines, L"PS", L"ps_6_5");
	(*mGlobalResources->Shaders)[L"DrawOitppllVS"] = make_unique<Shader>(L"shader\\oitppll.hlsl", nullDefines, L"VS", L"vs_6_5");
	(*mGlobalResources->Shaders)[L"DrawOitppllPS"] = make_unique<Shader>(L"shader\\oitppll.hlsl", nullDefines, L"PS", L"ps_6_5");
}

void Carol::OitppllManager::InitPSOs()
{
	auto buildStaticOitppllPsoDesc = *mGlobalResources->BasePsoDesc;
	auto buildStaicOitppllVS = (*mGlobalResources->Shaders)[L"BuildStaticOitppllVS"].get();
	auto buildStaticOitppllPS = (*mGlobalResources->Shaders)[L"BuildStaticOitppllPS"].get();
	buildStaticOitppllPsoDesc.VS = { reinterpret_cast<byte*>(buildStaicOitppllVS->GetBufferPointer()),buildStaicOitppllVS->GetBufferSize() };
	buildStaticOitppllPsoDesc.PS = { reinterpret_cast<byte*>(buildStaticOitppllPS->GetBufferPointer()),buildStaticOitppllPS->GetBufferSize() };
	buildStaticOitppllPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	buildStaticOitppllPsoDesc.DepthStencilState.DepthEnable = false;
	buildStaticOitppllPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	buildStaticOitppllPsoDesc.NumRenderTargets = 0;
	buildStaticOitppllPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	ThrowIfFailed(mGlobalResources->Device->CreateGraphicsPipelineState(&buildStaticOitppllPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"BuildStaticOitppll"].GetAddressOf())));

	auto buildSkinnedOitppllPsoDesc = buildStaticOitppllPsoDesc;
	auto buildSkinnedOitppllVS = (*mGlobalResources->Shaders)[L"BuildSkinnedOitppllVS"].get();
	auto buildSkinnedOitppllPS = (*mGlobalResources->Shaders)[L"BuildSkinnedOitppllPS"].get();
	buildSkinnedOitppllPsoDesc.VS = { reinterpret_cast<byte*>(buildSkinnedOitppllVS->GetBufferPointer()),buildSkinnedOitppllVS->GetBufferSize() };
	buildSkinnedOitppllPsoDesc.PS = { reinterpret_cast<byte*>(buildSkinnedOitppllPS->GetBufferPointer()),buildSkinnedOitppllPS->GetBufferSize() };
	ThrowIfFailed(mGlobalResources->Device->CreateGraphicsPipelineState(&buildSkinnedOitppllPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"BuildSkinnedOitppll"].GetAddressOf())));

	auto drawOitppllPsoDesc = *mGlobalResources->BasePsoDesc;
	auto drawStaicOitppllVS = (*mGlobalResources->Shaders)[L"DrawOitppllVS"].get();
	auto drawOitppllPS = (*mGlobalResources->Shaders)[L"DrawOitppllPS"].get();
	drawOitppllPsoDesc.VS = { reinterpret_cast<byte*>(drawStaicOitppllVS->GetBufferPointer()),drawStaicOitppllVS->GetBufferSize() };
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
	ThrowIfFailed(mGlobalResources->Device->CreateGraphicsPipelineState(&drawOitppllPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"DrawOitppll"].GetAddressOf())));
}

void Carol::OitppllManager::InitResources()
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

void Carol::OitppllManager::InitDescriptors()
{
	mGlobalResources->CbvSrvUavAllocator->CpuAllocate(OITPPLL_UAV_SRV_COUNT, mCpuCbvSrvUavAllocInfo.get());

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.StructureByteStride = sizeof(OitppllNode);
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = (*mGlobalResources->ClientWidth) * (*mGlobalResources->ClientHeight) * 16;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;

	mGlobalResources->Device->CreateUnorderedAccessView(mOitppllBuffer->Get(), mCounterBuffer->Get(), &uavDesc, GetCpuUav(PPLL_UAV));

	uavDesc.Buffer.StructureByteStride = 0;
	uavDesc.Buffer.NumElements = (*mGlobalResources->ClientWidth) * (*mGlobalResources->ClientHeight);
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
	uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	mGlobalResources->Device->CreateUnorderedAccessView(mStartOffsetBuffer->Get(), nullptr, &uavDesc, GetCpuUav(OFFSET_UAV));

	uavDesc.Buffer.NumElements = 1;
	mGlobalResources->Device->CreateUnorderedAccessView(mCounterBuffer->Get(), nullptr, &uavDesc, GetCpuUav(COUNTER_UAV));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.StructureByteStride = sizeof(OitppllNode);
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = (*mGlobalResources->ClientWidth) * (*mGlobalResources->ClientHeight) * 16;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;

	mGlobalResources->Device->CreateShaderResourceView(mOitppllBuffer->Get(), &srvDesc, GetCpuSrv(PPLL_SRV));

	srvDesc.Buffer.StructureByteStride = 0;
	srvDesc.Buffer.NumElements = (*mGlobalResources->ClientWidth) * (*mGlobalResources->ClientHeight);
	srvDesc.Format = DXGI_FORMAT_R32_UINT;
	mGlobalResources->Device->CreateShaderResourceView(mStartOffsetBuffer->Get(), &srvDesc, GetCpuSrv(OFFSET_SRV));
}

void Carol::OitppllManager::DrawPpll()
{
	mGlobalResources->CommandList->RSSetViewports(1, mGlobalResources->ScreenViewport);
    mGlobalResources->CommandList->RSSetScissorRects(1, mGlobalResources->ScissorRect);
	
	static const uint32_t initOffsetValue = UINT32_MAX;
	static const uint32_t initCounterValue = 0;
	mGlobalResources->CommandList->ClearUnorderedAccessViewUint(GetShaderGpuUav(OFFSET_UAV), GetCpuUav(OFFSET_UAV), mStartOffsetBuffer->Get(), &initOffsetValue, 0, nullptr);
	mGlobalResources->CommandList->ClearUnorderedAccessViewUint(GetShaderGpuUav(COUNTER_UAV), GetCpuUav(COUNTER_UAV), mCounterBuffer->Get(), &initCounterValue, 0, nullptr);
	mGlobalResources->CommandList->OMSetRenderTargets(0, nullptr, true, GetRvaluePtr(mGlobalResources->Display->GetDepthStencilView()));

	mGlobalResources->CommandList->SetGraphicsRootSignature(mGlobalResources->RootSignature);
    mGlobalResources->CommandList->SetGraphicsRootConstantBufferView(0, mGlobalResources->PassCBHeap->GetGPUVirtualAddress(mGlobalResources->PassCBAllocInfo));
	mGlobalResources->CommandList->SetGraphicsRootDescriptorTable(7, GetShaderGpuUav(PPLL_UAV));
	mGlobalResources->CommandList->SetGraphicsRootDescriptorTable(8, mGlobalResources->Display->GetDepthStencilSrv());

	mGlobalResources->DrawTransparentMeshes(
		(*mGlobalResources->PSOs)[L"BuildStaticOitppll"].Get(),
		(*mGlobalResources->PSOs)[L"BuildSkinnedOitppll"].Get(),
		true
	);
}

void Carol::OitppllManager::DrawOit()
{
	mGlobalResources->CommandList->OMSetRenderTargets(1, GetRvaluePtr(mGlobalResources->Taa->GetCurrFrameRtv()), true, GetRvaluePtr(mGlobalResources->Display->GetDepthStencilView()));
	mGlobalResources->CommandList->SetGraphicsRootConstantBufferView(0, mGlobalResources->PassCBHeap->GetGPUVirtualAddress(mGlobalResources->PassCBAllocInfo));
	mGlobalResources->CommandList->SetGraphicsRootDescriptorTable(4, GetShaderGpuSrv(PPLL_SRV));

	mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"DrawOitppll"].Get());
	mGlobalResources->CommandList->IASetVertexBuffers(0, 0, nullptr);
	mGlobalResources->CommandList->IASetIndexBuffer(nullptr);
	mGlobalResources->CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mGlobalResources->CommandList->DrawInstanced(6, 1, 0, 0);
}
