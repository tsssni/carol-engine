export module carol.renderer.dx12.pipeline_state;
import carol.renderer.dx12.shader;
import carol.renderer.dx12.root_signature;
import carol.renderer.dx12.command;
import carol.renderer.utils;
import <wrl/client.h>;
import <unordered_map>;
import <memory>;
import <string>;

namespace Carol
{
    using Microsoft::WRL::ComPtr;
    using std::unique_ptr;
    using std::unordered_map;
    using std::wstring;

    export enum PSOInitState
	{
		PSO_DEFAULT, PSO_EMPTY
	};

    export class PSO
	{
	public:
		ID3D12PipelineState* Get()const
        {
            return mPSO.Get();
        }

	protected:
		ComPtr<ID3D12PipelineState> mPSO;
	};

	export class MeshPSO : public PSO
	{
	public:
		MeshPSO(PSOInitState init = PSO_EMPTY)
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

		void SetRasterizerState(const D3D12_RASTERIZER_DESC& desc)
        {
            mPSODesc.RasterizerState = desc;
        }

		void SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& desc)
        {
            mPSODesc.DepthStencilState = desc;
        }

		void SetBlendState(const D3D12_BLEND_DESC& desc)
        {
            mPSODesc.BlendState = desc;
        }

		void SetDepthBias(uint32_t depthBias, float depthBiasClamp, float slopeScaledDepthBias)
        {
            mPSODesc.RasterizerState.DepthBias = depthBias;
            mPSODesc.RasterizerState.DepthBiasClamp = depthBiasClamp;
            mPSODesc.RasterizerState.SlopeScaledDepthBias = slopeScaledDepthBias;
        }

		void SetSampleMask(uint32_t mask)
        {
            mPSODesc.SampleMask = mask;
        }

		void SetNodeMask(uint32_t mask)
        {
            mPSODesc.NodeMask = mask;
        }

		void SetFlags(D3D12_PIPELINE_STATE_FLAGS flags)
        {
            mPSODesc.Flags = flags;
        }

		void SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE type)
        {
            mPSODesc.PrimitiveTopologyType = type;
        }

		void SetDepthTargetFormat(DXGI_FORMAT dsvFormat, uint32_t msaaCount = 1, uint32_t msaaQuality = 0)
        {
            SetRenderTargetFormat(0, nullptr, dsvFormat, msaaCount, msaaQuality);
        }

		void SetRenderTargetFormat(DXGI_FORMAT rtvFormat, uint32_t msaaCount = 1, uint32_t msaaQuality = 0)
        {
            SetRenderTargetFormat(1, &rtvFormat, DXGI_FORMAT_UNKNOWN, msaaCount, msaaQuality);
        }

		void SetRenderTargetFormat(DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat, uint32_t msaaCount = 1, uint32_t msaaQuality = 0)
        {
            SetRenderTargetFormat(1, &rtvFormat, dsvFormat, msaaCount, msaaQuality);
        }

		void SetRenderTargetFormat(uint32_t numRenderTargets, DXGI_FORMAT* rtvFormats, uint32_t msaaCount = 1, uint32_t msaaQuality = 0)
        {
            SetRenderTargetFormat(numRenderTargets, rtvFormats, DXGI_FORMAT_UNKNOWN, msaaCount, msaaQuality);
        }

		void SetRenderTargetFormat(uint32_t numRenderTargets, DXGI_FORMAT* rtvFormats, DXGI_FORMAT dsvFormat, uint32_t msaaCount = 1, uint32_t msaaQuality = 0)
        {
            mPSODesc.NumRenderTargets = numRenderTargets;
            mPSODesc.DSVFormat = dsvFormat;
            mPSODesc.SampleDesc.Count = msaaCount;
            mPSODesc.SampleDesc.Quality = msaaQuality;

            for (int i = 0; i < numRenderTargets; ++i)
            {
                mPSODesc.RTVFormats[i] = rtvFormats[0];
            }

        }

		void SetAS(const Shader* shader)
        {
            mPSODesc.AS = { reinterpret_cast<byte*>(shader->GetBufferPointer()), shader->GetBufferSize() };
        }

		void SetMS(const Shader* shader)
        {
            mPSODesc.MS = { reinterpret_cast<byte*>(shader->GetBufferPointer()), shader->GetBufferSize() };
        }

		void SetPS(const Shader* shader)
        {
            mPSODesc.PS = { reinterpret_cast<byte*>(shader->GetBufferPointer()),shader->GetBufferSize() };
        }

		void Finalize()
        {
            auto psoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(mPSODesc);
            D3D12_PIPELINE_STATE_STREAM_DESC desc;
            desc.pPipelineStateSubobjectStream = &psoStream;
            desc.SizeInBytes = sizeof(psoStream);
            
            static_cast<ID3D12Device2*>(gDevice.Get())->CreatePipelineState(&desc, IID_PPV_ARGS(mPSO.GetAddressOf()));
        }

	protected:
		D3DX12_MESH_SHADER_PIPELINE_STATE_DESC mPSODesc;
	};

	export class ComputePSO : public PSO
	{
	public:
		ComputePSO(PSOInitState init = PSO_EMPTY)
            :mPSODesc()
        {
            if (init == PSO_DEFAULT)
            {
                mPSODesc.pRootSignature = gRootSignature->Get();
                mPSODesc.NodeMask = 0;
                mPSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
            }
        }

		void SetNodeMask(uint32_t mask)
        {
            mPSODesc.NodeMask = mask;
        }

		void SetFlags(D3D12_PIPELINE_STATE_FLAGS flags)
        {
            mPSODesc.Flags = flags;
        }

		void SetCS(const Shader* shader)
        {
            mPSODesc.CS = { shader->GetBufferPointer(), shader->GetBufferSize() };
        }

		void Finalize()
        {
            gDevice->CreateComputePipelineState(&mPSODesc, IID_PPV_ARGS(mPSO.GetAddressOf()));
        }

	protected:
		D3D12_COMPUTE_PIPELINE_STATE_DESC mPSODesc;
	};
	
	export D3D12_RASTERIZER_DESC gCullDisabledState;
	export D3D12_DEPTH_STENCIL_DESC gDepthLessEqualState;
	export D3D12_DEPTH_STENCIL_DESC gDepthDisabledState;
	export D3D12_BLEND_DESC gAlphaBlendState;

    export unordered_map<wstring, unique_ptr<PSO>> gPSOs;
}