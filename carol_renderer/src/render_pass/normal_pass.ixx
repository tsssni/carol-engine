export module carol.renderer.render_pass.normal_pass;
import carol.renderer.render_pass.render_pass;
import carol.renderer.dx12;
import carol.renderer.scene;
import carol.renderer.utils;
import <DirectXColors.h>;
import <memory>;
import <vector>;
import <string_view>;

namespace Carol
{
    using std::make_unique;
    using std::unique_ptr;
    using std::vector;
    using std::wstring_view;

    export class NormalPass : public RenderPass
	{
	public:
		NormalPass(
            ID3D12Device* device,
            DXGI_FORMAT frameDsvFormat,
            DXGI_FORMAT normalMapFormat = DXGI_FORMAT_R8G8B8A8_SNORM)
        	:mNormalMapFormat(normalMapFormat),
            mFrameDsvFormat(frameDsvFormat),
            mIndirectCommandBuffer(MESH_TYPE_COUNT)
        {
            InitShaders();
            InitPSOs(device);
        }

		NormalPass(const NormalPass&) = delete;
		NormalPass(NormalPass&&) = delete;
		NormalPass& operator=(const NormalPass&) = delete;

		virtual void Draw(ID3D12GraphicsCommandList* cmdList)
        {
            cmdList->RSSetViewports(1, &mViewport);
            cmdList->RSSetScissorRects(1, &mScissorRect);

            cmdList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mNormalMap->Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)));
            cmdList->ClearRenderTargetView(mNormalMap->GetRtv(), DirectX::Colors::Blue, 0, nullptr);
            cmdList->ClearDepthStencilView(mFrameDsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);
            cmdList->OMSetRenderTargets(1, GetRvaluePtr(mNormalMap->GetRtv()), true, &mFrameDsv);

            cmdList->SetPipelineState(gPSOs[L"NormalsStatic"]->Get());
            ExecuteIndirect(cmdList, mIndirectCommandBuffer[OPAQUE_STATIC]);
            
            cmdList->SetPipelineState(gPSOs[L"NormalsSkinned"]->Get());
            ExecuteIndirect(cmdList, mIndirectCommandBuffer[OPAQUE_SKINNED]);

            cmdList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mNormalMap->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));
        }
        
        void SetIndirectCommandBuffer(MeshType type, const StructuredBuffer* indirectCommandBuffer)
        {
            mIndirectCommandBuffer[type] = indirectCommandBuffer;
        }
  
		void SetFrameDsv(D3D12_CPU_DESCRIPTOR_HANDLE frameDsv)
        {
            mFrameDsv = frameDsv;
        }

		uint32_t GetNormalSrvIdx()const
        {
            return mNormalMap->GetGpuSrvIdx();
        }

	protected:
		virtual void InitShaders()override
        {
            vector<wstring_view> nullDefines{};

            vector<wstring_view> skinnedDefines =
            {
                L"SKINNED=1"
            };

            if (gShaders.count(L"NormalsStaticMS") == 0)
            {
                gShaders[L"NormalsStaticMS"] = make_unique<Shader>(L"shader\\normals_ms.hlsl", nullDefines, L"main", L"ms_6_6");
            }

            if (gShaders.count(L"NormalsPS") == 0)
            {
                gShaders[L"NormalsPS"] = make_unique<Shader>(L"shader\\normals_ps.hlsl", nullDefines, L"main", L"ps_6_6");
            }

            if (gShaders.count(L"NormalsSkinnedMS") == 0)
            {
                gShaders[L"NormalsSkinnedMS"] = make_unique<Shader>(L"shader\\normals_ms.hlsl", skinnedDefines, L"main", L"ms_6_6");
            }
        }

		virtual void InitPSOs(ID3D12Device* device)override
        {
            if (gPSOs.count(L"NormalsStatic") == 0)
            {
                auto normalsStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
                normalsStaticMeshPSO->SetRootSignature(sRootSignature.get());
                normalsStaticMeshPSO->SetRenderTargetFormat(mNormalMapFormat, mFrameDsvFormat);
                normalsStaticMeshPSO->SetAS(gShaders[L"CullAS"].get());
                normalsStaticMeshPSO->SetMS(gShaders[L"NormalsStaticMS"].get());
                normalsStaticMeshPSO->SetPS(gShaders[L"NormalsPS"].get());
                normalsStaticMeshPSO->Finalize(device);

                gPSOs[L"NormalsStatic"] = std::move(normalsStaticMeshPSO);
            }

            if (gPSOs.count(L"NormalsSkinned") == 0)
            {
                auto normalsSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
                normalsSkinnedMeshPSO->SetRootSignature(sRootSignature.get());
                normalsSkinnedMeshPSO->SetRenderTargetFormat(mNormalMapFormat, mFrameDsvFormat);
                normalsSkinnedMeshPSO->SetAS(gShaders[L"CullAS"].get());
                normalsSkinnedMeshPSO->SetMS(gShaders[L"NormalsSkinnedMS"].get());
                normalsSkinnedMeshPSO->SetPS(gShaders[L"NormalsPS"].get());
                normalsSkinnedMeshPSO->Finalize(device);
                
                gPSOs[L"NormalsSkinned"] = std::move(normalsSkinnedMeshPSO);
            }
        }

		virtual void InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)override
        {
            D3D12_CLEAR_VALUE optClearValue  = CD3DX12_CLEAR_VALUE(mNormalMapFormat, DirectX::Colors::Blue);
            mNormalMap = make_unique<ColorBuffer>(
                mWidth,
                mHeight,
                1,
                COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
                mNormalMapFormat,
                device,
                heap,
                descriptorManager,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                &optClearValue);
        }

		unique_ptr<ColorBuffer> mNormalMap;
		DXGI_FORMAT mNormalMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

        vector<const StructuredBuffer*> mIndirectCommandBuffer;
		D3D12_CPU_DESCRIPTOR_HANDLE mFrameDsv;
        DXGI_FORMAT mFrameDsvFormat;
    };
}