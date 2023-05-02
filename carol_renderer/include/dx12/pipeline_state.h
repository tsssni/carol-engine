#pragma once
#include <utils/d3dx12.h>
#include <wrl/client.h>

namespace Carol
{
	class Shader;
	class RootSignature;

	class PSO
	{
	public:
		ID3D12PipelineState* Get()const;
	protected:
		Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;
	};

	enum PSOInitState
	{
		PSO_DEFAULT, PSO_EMPTY
	};

	class MeshPSO : public PSO
	{
	public:
		MeshPSO(PSOInitState init = PSO_EMPTY);

		void SetRootSignature(const RootSignature* rootSignature);
		void SetRasterizerState(const D3D12_RASTERIZER_DESC* desc);
		void SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC* desc);
		void SetBlendState(const D3D12_BLEND_DESC* desc);
		void SetDepthBias(uint32_t depthBias, float depthBiasClamp, float slopeScaledDepthBias);
		void SetSampleMask(uint32_t mask);
		void SetNodeMask(uint32_t mask);
		void SetFlags(D3D12_PIPELINE_STATE_FLAGS flags);
		void SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE type);
		void SetDepthTargetFormat(DXGI_FORMAT dsvFormat, uint32_t msaaCount = 1, uint32_t msaaQuality = 0);
		void SetRenderTargetFormat(DXGI_FORMAT rtvFormat, uint32_t msaaCount = 1, uint32_t msaaQuality = 0);
		void SetRenderTargetFormat(DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat, uint32_t msaaCount = 1, uint32_t msaaQuality = 0);
		void SetRenderTargetFormat(uint32_t numRenderTargets, DXGI_FORMAT* rtvFormats, uint32_t msaaCount = 1, uint32_t msaaQuality = 0);
		void SetRenderTargetFormat(uint32_t numRenderTargets, DXGI_FORMAT* rtvFormats, DXGI_FORMAT dsvFormat, uint32_t msaaCount = 1, uint32_t msaaQuality = 0);

		void SetAS(const Shader* shader);
		void SetMS(const Shader* shader);
		void SetPS(const Shader* shader);

		void Finalize();
	protected:
		D3DX12_MESH_SHADER_PIPELINE_STATE_DESC mPSODesc;
	};

	class ComputePSO : public PSO
	{
	public:
		ComputePSO(PSOInitState init = PSO_EMPTY);

		void SetRootSignature(const RootSignature* rootSignature);
		void SetNodeMask(uint32_t mask);
		void SetFlags(D3D12_PIPELINE_STATE_FLAGS flags);

		void SetCS(const Shader* shader);

		void Finalize();
	protected:
		D3D12_COMPUTE_PIPELINE_STATE_DESC mPSODesc;
	};
}
