#include <dx12/pipeline_state.h>
#include <dx12/root_signature.h>
#include <dx12/shader.h>
#include <global.h>

namespace Carol
{
	using Microsoft::WRL::ComPtr;
}

ID3D12PipelineState* Carol::PSO::Get()const
{
	return mPSO.Get();
}

Carol::MeshPSO::MeshPSO(PSOInitState init)
	:mPSODesc()
{
	if (init == PSO_DEFAULT)
	{
		mPSODesc.pRootSignature = gRootSignature->Get();
		mPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		mPSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		mPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		mPSODesc.SampleMask = UINT_MAX;
		mPSODesc.NodeMask = 0;
		mPSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		mPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	}
}

void Carol::MeshPSO::SetRootSignature(const RootSignature* rootSignature)
{
	mPSODesc.pRootSignature = rootSignature->Get();
}

void Carol::MeshPSO::SetRasterizerState(const D3D12_RASTERIZER_DESC* desc)
{
	mPSODesc.RasterizerState = *desc;
}

void Carol::MeshPSO::SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC* desc)
{
	mPSODesc.DepthStencilState = *desc;
}

void Carol::MeshPSO::SetBlendState(const D3D12_BLEND_DESC* desc)
{
	mPSODesc.BlendState = *desc;
}

void Carol::MeshPSO::SetDepthBias(uint32_t depthBias, float depthBiasClamp, float slopeScaledDepthBias)
{
	mPSODesc.RasterizerState.DepthBias = depthBias;
	mPSODesc.RasterizerState.DepthBiasClamp = depthBiasClamp;
	mPSODesc.RasterizerState.SlopeScaledDepthBias = slopeScaledDepthBias;
}

void Carol::MeshPSO::SetSampleMask(uint32_t mask)
{
	mPSODesc.SampleMask = mask;
}

void Carol::MeshPSO::SetNodeMask(uint32_t mask)
{
	mPSODesc.NodeMask = mask;
}

void Carol::MeshPSO::SetFlags(D3D12_PIPELINE_STATE_FLAGS flags)
{
	mPSODesc.Flags = flags;
}

void Carol::MeshPSO::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE type)
{
	mPSODesc.PrimitiveTopologyType = type;
}

void Carol::MeshPSO::SetDepthTargetFormat(DXGI_FORMAT dsvFormat, uint32_t msaaCount, uint32_t msaaQuality)
{
	SetRenderTargetFormat(0, nullptr, dsvFormat, msaaCount, msaaQuality);
}

void Carol::MeshPSO::SetRenderTargetFormat(DXGI_FORMAT rtvFormat, uint32_t msaaCount, uint32_t msaaQuality)
{
	SetRenderTargetFormat(1, &rtvFormat, DXGI_FORMAT_UNKNOWN, msaaCount, msaaQuality);
}

void Carol::MeshPSO::SetRenderTargetFormat(DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat, uint32_t msaaCount, uint32_t msaaQuality)
{
	SetRenderTargetFormat(1, &rtvFormat, dsvFormat, msaaCount, msaaQuality);
}

void Carol::MeshPSO::SetRenderTargetFormat(uint32_t numRenderTargets, DXGI_FORMAT* rtvFormats, uint32_t msaaCount, uint32_t msaaQuality)
{
	SetRenderTargetFormat(numRenderTargets, rtvFormats, DXGI_FORMAT_UNKNOWN, msaaCount, msaaQuality);
}

void Carol::MeshPSO::SetRenderTargetFormat(uint32_t numRenderTargets, DXGI_FORMAT* rtvFormats, DXGI_FORMAT dsvFormat, uint32_t msaaCount, uint32_t msaaQuality)
{
	mPSODesc.NumRenderTargets = numRenderTargets;
	mPSODesc.DSVFormat = dsvFormat;
	mPSODesc.SampleDesc.Count = msaaCount;
	mPSODesc.SampleDesc.Quality = msaaQuality;

	for (int i = 0; i < numRenderTargets; ++i)
	{
		mPSODesc.RTVFormats[i] = rtvFormats[i];
	}

}

void Carol::MeshPSO::SetAS(const Shader* shader)
{
	mPSODesc.AS = { reinterpret_cast<byte*>(shader->GetBufferPointer()), shader->GetBufferSize() };
}

void Carol::MeshPSO::SetMS(const Shader* shader)
{
	mPSODesc.MS = { reinterpret_cast<byte*>(shader->GetBufferPointer()), shader->GetBufferSize() };
}

void Carol::MeshPSO::SetPS(const Shader* shader)
{
	mPSODesc.PS = { reinterpret_cast<byte*>(shader->GetBufferPointer()),shader->GetBufferSize() };
}

void Carol::MeshPSO::Finalize()
{
	auto psoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(mPSODesc);
	D3D12_PIPELINE_STATE_STREAM_DESC desc;
	desc.pPipelineStateSubobjectStream = &psoStream;
	desc.SizeInBytes = sizeof(psoStream);

	static_cast<ID3D12Device2*>(gDevice.Get())->CreatePipelineState(&desc, IID_PPV_ARGS(mPSO.GetAddressOf()));
}

Carol::ComputePSO::ComputePSO(PSOInitState init)
	:mPSODesc()
{
	if (init == PSO_DEFAULT)
	{
		mPSODesc.pRootSignature = gRootSignature->Get();
		mPSODesc.NodeMask = 0;
		mPSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	}
}

void Carol::ComputePSO::SetRootSignature(const RootSignature* rootSignature)
{
	mPSODesc.pRootSignature = rootSignature->Get();
}

void Carol::ComputePSO::SetNodeMask(uint32_t mask)
{
	mPSODesc.NodeMask = mask;
}

void Carol::ComputePSO::SetFlags(D3D12_PIPELINE_STATE_FLAGS flags)
{
	mPSODesc.Flags = flags;
}

void Carol::ComputePSO::SetCS(const Shader* shader)
{
	mPSODesc.CS = { shader->GetBufferPointer(), shader->GetBufferSize() };
}

void Carol::ComputePSO::Finalize()
{
	gDevice->CreateComputePipelineState(&mPSODesc, IID_PPV_ARGS(mPSO.GetAddressOf()));
}
