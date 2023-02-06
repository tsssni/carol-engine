export module carol.renderer.render_pass.normal_pass;
import carol.renderer.render_pass.render_pass;
import carol.renderer.render_pass.display_pass;
import carol.renderer.render_pass.frame_pass;
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
		NormalPass(DXGI_FORMAT normalMapFormat = DXGI_FORMAT_R8G8B8A8_SNORM)
        	:mNormalMapFormat(normalMapFormat)
        {
            InitShaders();
            InitPSOs();
        }

		NormalPass(const NormalPass&) = delete;
		NormalPass(NormalPass&&) = delete;
		NormalPass& operator=(const NormalPass&) = delete;

		virtual void Draw()override
        {
            gCommandList->RSSetViewports(1, &mViewport);
            gCommandList->RSSetScissorRects(1, &mScissorRect);

            gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mNormalMap->Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)));
            gCommandList->ClearRenderTargetView(mNormalMap->GetRtv(), DirectX::Colors::Blue, 0, nullptr);
            gCommandList->ClearDepthStencilView(gFramePass->GetFrameDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);
            gCommandList->OMSetRenderTargets(1, GetRvaluePtr(mNormalMap->GetRtv()), true, GetRvaluePtr(gFramePass->GetFrameDsv()));

            gCommandList->SetPipelineState(gPSOs[L"NormalsStatic"]->Get());
            gScene->ExecuteIndirect(gFramePass->GetIndirectCommandBuffer(OPAQUE_STATIC));
            
            gCommandList->SetPipelineState(gPSOs[L"NormalsSkinned"]->Get());
            gScene->ExecuteIndirect(gFramePass->GetIndirectCommandBuffer(OPAQUE_SKINNED));

            gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mNormalMap->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));
        }

		virtual void Update()override
        {

        }

		uint32_t GetNormalSrvIdx()
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

		virtual void InitPSOs()override
        {
            if (gPSOs.count(L"NormalsStatic") == 0)
            {
                auto normalsStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
                normalsStaticMeshPSO->SetRenderTargetFormat(mNormalMapFormat, gFramePass->GetFrameDsvFormat());
                normalsStaticMeshPSO->SetAS(gShaders[L"CullAS"].get());
                normalsStaticMeshPSO->SetMS(gShaders[L"NormalsStaticMS"].get());
                normalsStaticMeshPSO->SetPS(gShaders[L"NormalsPS"].get());
                normalsStaticMeshPSO->Finalize();

                gPSOs[L"NormalsStatic"] = std::move(normalsStaticMeshPSO);
            }

            if (gPSOs.count(L"NormalsSkinned") == 0)
            {
                auto normalsSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
                normalsSkinnedMeshPSO->SetRenderTargetFormat(mNormalMapFormat, gFramePass->GetFrameDsvFormat());
                normalsSkinnedMeshPSO->SetAS(gShaders[L"CullAS"].get());
                normalsSkinnedMeshPSO->SetMS(gShaders[L"NormalsSkinnedMS"].get());
                normalsSkinnedMeshPSO->SetPS(gShaders[L"NormalsPS"].get());
                normalsSkinnedMeshPSO->Finalize();
                
                gPSOs[L"NormalsSkinned"] = std::move(normalsSkinnedMeshPSO);
            }
        }

		virtual void InitBuffers()override
        {
            D3D12_CLEAR_VALUE optClearValue  = CD3DX12_CLEAR_VALUE(mNormalMapFormat, DirectX::Colors::Blue);
            mNormalMap = make_unique<ColorBuffer>(
                mWidth,
                mHeight,
                1,
                COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
                mNormalMapFormat,
                gHeapManager->GetDefaultBuffersHeap(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                &optClearValue);
        }

		unique_ptr<ColorBuffer> mNormalMap;
		DXGI_FORMAT mNormalMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	};
}