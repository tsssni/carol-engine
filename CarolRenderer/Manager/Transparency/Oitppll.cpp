#include "Oitppll.h"
#include "../Display/Display.h"
#include "../Anti-Aliasing/Taa.h"
#include "../../DirectX/Resource.h"
#include "../../DirectX/Heap.h"
#include "../../DirectX/DescriptorAllocator.h"
#include "../../DirectX/Shader.h"
#include "../../DirectX/Sampler.h"
#include <vector>

using std::vector;
using std::make_unique;

Carol::OitppllManager::OitppllManager(RenderData* renderData, DXGI_FORMAT outputFormat)
	:Manager(renderData), mOutputFormat(outputFormat)
{
	InitRootSignature();
	InitShaders();
	InitPSOs();
	OnResize();
}

void Carol::OitppllManager::Draw()
{
	if (mRenderData->TransparentStaticMeshes->size() == 0 
		&& mRenderData->TransparentSkinnedMeshes->size() == 0)
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

    if (width != *mRenderData->ClientWidth || height != *mRenderData->ClientHeight)
    {
        Manager::OnResize();

        width = *mRenderData->ClientWidth;
        height = *mRenderData->ClientHeight;

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
	const D3D_SHADER_MACRO staticDefines[] =
	{
		"TAA","1",
		"SSAO","1",
		NULL,NULL
	};

	const D3D_SHADER_MACRO skinnedDefines[] =
	{
		"TAA","1",
		"SSAO","1",
		"SKINNED","1",
		NULL,NULL
	};

	(*mRenderData->Shaders)[L"BuildStaticOitppllVS"] = make_unique<Shader>(L"Shaders\\BuildOitppll.hlsl", staticDefines, "VS", "vs_5_1");
	(*mRenderData->Shaders)[L"BuildStaticOitppllPS"] = make_unique<Shader>(L"Shaders\\BuildOitppll.hlsl", staticDefines, "PS", "ps_5_1");
	(*mRenderData->Shaders)[L"BuildSkinnedOitppllVS"] = make_unique<Shader>(L"Shaders\\BuildOitppll.hlsl", skinnedDefines, "VS", "vs_5_1");
	(*mRenderData->Shaders)[L"BuildSkinnedOitppllPS"] = make_unique<Shader>(L"Shaders\\BuildOitppll.hlsl", skinnedDefines, "PS", "ps_5_1");
	(*mRenderData->Shaders)[L"DrawOitppllVS"] = make_unique<Shader>(L"Shaders\\DrawOitppll.hlsl", nullptr, "VS", "vs_5_1");
	(*mRenderData->Shaders)[L"DrawOitppllPS"] = make_unique<Shader>(L"Shaders\\DrawOitppll.hlsl", nullptr, "PS", "ps_5_1");
}

void Carol::OitppllManager::InitPSOs()
{
	auto buildStaticOitppllPsoDesc = *mRenderData->BasePsoDesc;
	auto buildStaicOitppllVSBlob = (*mRenderData->Shaders)[L"BuildStaticOitppllVS"]->GetBlob()->Get();
	auto buildStaticOitppllPSBlob = (*mRenderData->Shaders)[L"BuildStaticOitppllPS"]->GetBlob()->Get();
	buildStaticOitppllPsoDesc.VS = { reinterpret_cast<byte*>(buildStaicOitppllVSBlob->GetBufferPointer()),buildStaicOitppllVSBlob->GetBufferSize() };
	buildStaticOitppllPsoDesc.PS = { reinterpret_cast<byte*>(buildStaticOitppllPSBlob->GetBufferPointer()),buildStaticOitppllPSBlob->GetBufferSize() };
	buildStaticOitppllPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	buildStaticOitppllPsoDesc.DepthStencilState.DepthEnable = false;
	buildStaticOitppllPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	buildStaticOitppllPsoDesc.NumRenderTargets = 0;
	buildStaticOitppllPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	ThrowIfFailed(mRenderData->Device->CreateGraphicsPipelineState(&buildStaticOitppllPsoDesc, IID_PPV_ARGS((*mRenderData->PSOs)[L"BuildStaticOitppll"].GetAddressOf())));

	auto buildSkinnedOitppllPsoDesc = buildStaticOitppllPsoDesc;
	auto buildSkinnedOitppllVSBlob = (*mRenderData->Shaders)[L"BuildSkinnedOitppllVS"]->GetBlob()->Get();
	auto buildSkinnedOitppllPSBlob = (*mRenderData->Shaders)[L"BuildSkinnedOitppllPS"]->GetBlob()->Get();
	buildSkinnedOitppllPsoDesc.VS = { reinterpret_cast<byte*>(buildSkinnedOitppllVSBlob->GetBufferPointer()),buildSkinnedOitppllVSBlob->GetBufferSize() };
	buildSkinnedOitppllPsoDesc.PS = { reinterpret_cast<byte*>(buildSkinnedOitppllPSBlob->GetBufferPointer()),buildSkinnedOitppllPSBlob->GetBufferSize() };
	ThrowIfFailed(mRenderData->Device->CreateGraphicsPipelineState(&buildSkinnedOitppllPsoDesc, IID_PPV_ARGS((*mRenderData->PSOs)[L"BuildSkinnedOitppll"].GetAddressOf())));

	auto drawOitppllPsoDesc = *mRenderData->BasePsoDesc;
	auto drawStaicOitppllVSBlob = (*mRenderData->Shaders)[L"DrawOitppllVS"]->GetBlob()->Get();
	auto drawOitppllPSBlob = (*mRenderData->Shaders)[L"DrawOitppllPS"]->GetBlob()->Get();
	drawOitppllPsoDesc.VS = { reinterpret_cast<byte*>(drawStaicOitppllVSBlob->GetBufferPointer()),drawStaicOitppllVSBlob->GetBufferSize() };
	drawOitppllPsoDesc.PS = { reinterpret_cast<byte*>(drawOitppllPSBlob->GetBufferPointer()),drawOitppllPSBlob->GetBufferSize() };
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
	ThrowIfFailed(mRenderData->Device->CreateGraphicsPipelineState(&drawOitppllPsoDesc, IID_PPV_ARGS((*mRenderData->PSOs)[L"DrawOitppll"].GetAddressOf())));
}

void Carol::OitppllManager::InitResources()
{
	uint32_t linkedListBufferSize = (*mRenderData->ClientWidth) * (*mRenderData->ClientHeight) * 16 * sizeof(OitppllNode);
	auto linkedListBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(linkedListBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	mOitppllBuffer = make_unique<DefaultResource>(&linkedListBufferDesc, mRenderData->DefaultBuffersHeap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	uint32_t startOffsetBufferSize = (*mRenderData->ClientWidth) * (*mRenderData->ClientHeight) * sizeof(uint32_t);
	auto startOffsetBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(startOffsetBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	mStartOffsetBuffer = make_unique<DefaultResource>(&startOffsetBufferDesc, mRenderData->DefaultBuffersHeap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	
	uint32_t counterBufferSize = sizeof(uint32_t);
	auto counterBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(counterBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	mCounterBuffer = make_unique<DefaultResource>(&counterBufferDesc, mRenderData->DefaultBuffersHeap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	InitDescriptors();
}

void Carol::OitppllManager::InitDescriptors()
{
	mRenderData->CbvSrvUavAllocator->CpuAllocate(OITPPLL_UAV_SRV_COUNT, mCpuCbvSrvUavAllocInfo.get());

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.StructureByteStride = sizeof(OitppllNode);
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = (*mRenderData->ClientWidth) * (*mRenderData->ClientHeight) * 16;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;

	mRenderData->Device->CreateUnorderedAccessView(mOitppllBuffer->Get(), mCounterBuffer->Get(), &uavDesc, GetCpuUav(PPLL_UAV));

	uavDesc.Buffer.StructureByteStride = 0;
	uavDesc.Buffer.NumElements = (*mRenderData->ClientWidth) * (*mRenderData->ClientHeight);
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
	uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	mRenderData->Device->CreateUnorderedAccessView(mStartOffsetBuffer->Get(), nullptr, &uavDesc, GetCpuUav(OFFSET_UAV));

	uavDesc.Buffer.NumElements = 1;
	mRenderData->Device->CreateUnorderedAccessView(mCounterBuffer->Get(), nullptr, &uavDesc, GetCpuUav(COUNTER_UAV));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.StructureByteStride = sizeof(OitppllNode);
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = (*mRenderData->ClientWidth) * (*mRenderData->ClientHeight) * 16;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;

	mRenderData->Device->CreateShaderResourceView(mOitppllBuffer->Get(), &srvDesc, GetCpuSrv(PPLL_SRV));

	srvDesc.Buffer.StructureByteStride = 0;
	srvDesc.Buffer.NumElements = (*mRenderData->ClientWidth) * (*mRenderData->ClientHeight);
	srvDesc.Format = DXGI_FORMAT_R32_UINT;
	mRenderData->Device->CreateShaderResourceView(mStartOffsetBuffer->Get(), &srvDesc, GetCpuSrv(OFFSET_SRV));
}

void Carol::OitppllManager::DrawPpll()
{
	mRenderData->CommandList->RSSetViewports(1, mRenderData->ScreenViewport);
    mRenderData->CommandList->RSSetScissorRects(1, mRenderData->ScissorRect);
	
	static const uint32_t initOffsetValue = UINT32_MAX;
	static const uint32_t initCounterValue = 0;
	mRenderData->CommandList->ClearUnorderedAccessViewUint(GetShaderGpuUav(OFFSET_UAV), GetCpuUav(OFFSET_UAV), mStartOffsetBuffer->Get(), &initOffsetValue, 0, nullptr);
	mRenderData->CommandList->ClearUnorderedAccessViewUint(GetShaderGpuUav(COUNTER_UAV), GetCpuUav(COUNTER_UAV), mCounterBuffer->Get(), &initCounterValue, 0, nullptr);
	mRenderData->CommandList->OMSetRenderTargets(0, nullptr, true, GetRvaluePtr(mRenderData->Display->GetDepthStencilView()));

	mRenderData->CommandList->SetGraphicsRootSignature(mRenderData->RootSignature);
    mRenderData->CommandList->SetGraphicsRootConstantBufferView(0, mRenderData->PassCBHeap->GetGPUVirtualAddress(mRenderData->PassCBAllocInfo));
	mRenderData->CommandList->SetGraphicsRootDescriptorTable(7, GetShaderGpuUav(PPLL_UAV));
	mRenderData->CommandList->SetGraphicsRootDescriptorTable(8, mRenderData->Display->GetDepthStencilSrv());

	mRenderData->DrawTransparentMeshes(
		(*mRenderData->PSOs)[L"BuildStaticOitppll"].Get(),
		(*mRenderData->PSOs)[L"BuildSkinnedOitppll"].Get(),
		true
	);
}

void Carol::OitppllManager::DrawOit()
{
	mRenderData->CommandList->OMSetRenderTargets(1, GetRvaluePtr(mRenderData->Taa->GetCurrFrameRtv()), true, GetRvaluePtr(mRenderData->Display->GetDepthStencilView()));
	mRenderData->CommandList->SetGraphicsRootConstantBufferView(0, mRenderData->PassCBHeap->GetGPUVirtualAddress(mRenderData->PassCBAllocInfo));
	mRenderData->CommandList->SetGraphicsRootDescriptorTable(4, GetShaderGpuSrv(PPLL_SRV));

	mRenderData->CommandList->SetPipelineState((*mRenderData->PSOs)[L"DrawOitppll"].Get());
	mRenderData->CommandList->IASetVertexBuffers(0, 0, nullptr);
	mRenderData->CommandList->IASetIndexBuffer(nullptr);
	mRenderData->CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mRenderData->CommandList->DrawInstanced(6, 1, 0, 0);
}
