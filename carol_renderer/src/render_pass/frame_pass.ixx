export module carol.renderer.render_pass.frame_pass;
import carol.renderer.render_pass.render_pass;
import carol.renderer.dx12;
import carol.renderer.scene;
import carol.renderer.utils;
import <DirectXMath.h>;
import <DirectXColors.h>;
import <memory>;
import <vector>;
import <string>;
import <string_view>;
import <span>;
import <cmath>;

#define MAX_LIGHTS 16

namespace Carol
{
    using std::make_unique;
    using std::span;
    using std::unique_ptr;
    using std::vector;
    using std::wstring;
    using std::wstring_view;
    using namespace DirectX;

    class OitppllNode
    {
    public:
        XMFLOAT4 Color;
        uint32_t Depth;
        uint32_t Next;
    };

    export class FramePass : public RenderPass
    {
    public:
        FramePass(
            ID3D12Device* device,
            Heap* heap,
            DescriptorManager* descriptorManager,
            Scene* scene,
            DXGI_FORMAT frameFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
            DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
            DXGI_FORMAT hiZFormat = DXGI_FORMAT_R32_FLOAT)
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

        FramePass(const FramePass &) = delete;
        FramePass(FramePass &&) = delete;
        FramePass &operator=(const FramePass &) = delete;

        virtual void Draw(ID3D12GraphicsCommandList* cmdList) override
        {
            cmdList->RSSetViewports(1, &mViewport);
            cmdList->RSSetScissorRects(1, &mScissorRect);

            mFrameMap->Transition(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
            cmdList->ClearRenderTargetView(GetFrameRtv(), Colors::Gray, 0, nullptr);
            cmdList->ClearDepthStencilView(GetFrameDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);
            cmdList->OMSetRenderTargets(1, GetRvaluePtr(GetFrameRtv()), true, GetRvaluePtr(GetFrameDsv()));

            DrawOpaque(cmdList);
            DrawTransparent(cmdList);
            
            mFrameMap->Transition(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        void Cull(ID3D12GraphicsCommandList* cmdList)
        {
            Clear(cmdList);
            cmdList->RSSetViewports(1, &mViewport);
            cmdList->RSSetScissorRects(1, &mScissorRect);

            GenerateHiZ(cmdList);
            CullInstances(cmdList, true, true);
            CullMeshlets(cmdList, true, true);

            GenerateHiZ(cmdList);
            CullInstances(cmdList, false, true);
            CullMeshlets(cmdList, false, true);

            CullInstances(cmdList, false, false);
            CullMeshlets(cmdList, false, false);
        }

        void Update(uint64_t cpuFenceValue, uint64_t completedFenceValue)
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
            mHiZIdx[HIZ_R_IDX] = mHiZMap->GetGpuSrvIdx();
            mHiZIdx[HIZ_W_IDX] = mHiZMap->GetGpuUavIdx();
        }

        D3D12_CPU_DESCRIPTOR_HANDLE GetFrameRtv()const
        {
            return mFrameMap->GetRtv();
        }

        D3D12_CPU_DESCRIPTOR_HANDLE GetFrameDsv()const
        {
            return mDepthStencilMap->GetDsv();
        }

        DXGI_FORMAT GetFrameFormat()const
        {
            return mFrameFormat;
        }

        DXGI_FORMAT GetFrameDsvFormat()const
        {
            return GetDsvFormat(mDepthStencilFormat);
        }

        const StructuredBuffer *GetIndirectCommandBuffer(MeshType type)const
        {
            return mCulledCommandBuffer[type].get();
        }

        uint32_t GetFrameSrvIdx()const
        {
            return mFrameMap->GetGpuSrvIdx();
        }

        uint32_t GetDepthStencilSrvIdx()const
        {
            return mDepthStencilMap->GetGpuSrvIdx();
        }

        uint32_t GetHiZSrvIdx()const
        {
            return mHiZMap->GetGpuSrvIdx();
        }

        uint32_t GetHiZUavIdx()const
        {
            return mHiZMap->GetGpuUavIdx();
        }

        uint32_t GetPpllUavIdx()const
        {
            return mOitppllBuffer->GetGpuUavIdx();
        }

        uint32_t GetOffsetUavIdx()const
        {
            return mStartOffsetBuffer->GetGpuUavIdx();
        }

        uint32_t GetCounterUavIdx()const
        {
            return mCounterBuffer->GetGpuUavIdx();
        }

        uint32_t GetPpllSrvIdx()const
        {
            return mOitppllBuffer->GetGpuSrvIdx();
        }

        uint32_t GetOffsetSrvIdx()const
        {
            return mStartOffsetBuffer->GetGpuSrvIdx();
        }

    protected:
        virtual void InitShaders() override
        {
            vector<wstring_view> nullDefines = {};

            vector<wstring_view> staticDefines = 
            {
                L"TAA=1",L"SSAO=1"
            };

            vector<wstring_view> skinnedDefines =
            {
                L"TAA=1",L"SSAO=1",L"SKINNED=1"
            };

            vector<wstring_view> cullDefines =
            {
                L"OCCLUSION=1"
            };

            vector<wstring_view> cullWriteDefines =
            {
                L"OCCLUSION=1",
                L"WRITE=1"
            };

            vector<wstring_view> meshShaderDisabledCullWriteDefines =
            {
                L"OCCLUSION=1",
                L"WRITE=1",
                L"MESH_SHADER_DISABLED=1"
            };
            
            if (gShaders.count(L"OpaqueStaticMS") == 0)
            {
                gShaders[L"OpaqueStaticMS"] = make_unique<Shader>(L"shader\\opaque_ms.hlsl", staticDefines, L"main", L"ms_6_6");
            }

            if (gShaders.count(L"OpaquePS") == 0)
            {
                gShaders[L"OpaquePS"] = make_unique<Shader>(L"shader\\opaque_ps.hlsl", staticDefines, L"main", L"ps_6_6");
            }

            if (gShaders.count(L"OpaqueSkinnedMS") == 0)
            {
                gShaders[L"OpaqueSkinnedMS"] = make_unique<Shader>(L"shader\\opaque_ms.hlsl", skinnedDefines, L"main", L"ms_6_6");
            }

            if (gShaders.count(L"ScreenMS") == 0)
            {
                gShaders[L"ScreenMS"] = make_unique<Shader>(L"shader\\screen_ms.hlsl", nullDefines, L"main", L"ms_6_6");
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
                gShaders[L"FrameCullCS"] = make_unique<Shader>(L"shader\\cull_cs.hlsl", cullDefines, L"main", L"cs_6_6");
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

            if (gShaders.count(L"BuildOitppllPS") == 0)
            {
                gShaders[L"BuildOitppllPS"] = make_unique<Shader>(L"shader\\oitppll_build_ps.hlsl", staticDefines, L"main", L"ps_6_6");
            }

            if (gShaders.count(L"DrawOitppllPS") == 0)
            {
                gShaders[L"DrawOitppllPS"] = make_unique<Shader>(L"shader\\oitppll_ps.hlsl", nullDefines, L"main", L"ps_6_6");
            }

            if (gShaders.count(L"TransparentCullWriteAS") == 0)
            {
                gShaders[L"TransparentCullWriteAS"] = make_unique<Shader>(L"shader\\cull_as.hlsl", meshShaderDisabledCullWriteDefines, L"main", L"as_6_6");
            }
        }

        virtual void InitPSOs(ID3D12Device* device) override
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

            if (gPSOs.count(L"OpaqueStatic") == 0)
            {
                auto opaqueStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
                opaqueStaticMeshPSO->SetRootSignature(sRootSignature.get());
                opaqueStaticMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
                opaqueStaticMeshPSO->SetAS(gShaders[L"CullAS"].get());
                opaqueStaticMeshPSO->SetMS(gShaders[L"OpaqueStaticMS"].get());
                opaqueStaticMeshPSO->SetPS(gShaders[L"OpaquePS"].get());
                opaqueStaticMeshPSO->Finalize(device);
            
                gPSOs[L"OpaqueStatic"] = std::move(opaqueStaticMeshPSO);
            }

            if (gPSOs.count(L"OpaqueSkinned") == 0)
            {
                auto opaqueSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
                opaqueSkinnedMeshPSO->SetRootSignature(sRootSignature.get());
                opaqueSkinnedMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
                opaqueSkinnedMeshPSO->SetAS(gShaders[L"CullAS"].get());
                opaqueSkinnedMeshPSO->SetMS(gShaders[L"OpaqueSkinnedMS"].get());
                opaqueSkinnedMeshPSO->SetPS(gShaders[L"OpaquePS"].get());
                opaqueSkinnedMeshPSO->Finalize(device);
            
                gPSOs[L"OpaqueSkinned"] = std::move(opaqueSkinnedMeshPSO);
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

            if (gPSOs.count(L"BuildStaticOitppll") == 0)
            {
                auto buildStaticOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
                buildStaticOitppllMeshPSO->SetRootSignature(sRootSignature.get());
                buildStaticOitppllMeshPSO->SetRasterizerState(gCullDisabledState);
                buildStaticOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState);
                buildStaticOitppllMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
                buildStaticOitppllMeshPSO->SetAS(gShaders[L"CullAS"].get());
                buildStaticOitppllMeshPSO->SetMS(gShaders[L"OpaqueStaticMS"].get());
                buildStaticOitppllMeshPSO->SetPS(gShaders[L"BuildOitppllPS"].get());
                buildStaticOitppllMeshPSO->Finalize(device);
            
                gPSOs[L"BuildStaticOitppll"] = std::move(buildStaticOitppllMeshPSO);
            }

            if (gPSOs.count(L"BuildSkinnedOitppll") == 0)
            {
                auto buildSkinnedOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
                buildSkinnedOitppllMeshPSO->SetRootSignature(sRootSignature.get());
                buildSkinnedOitppllMeshPSO->SetRasterizerState(gCullDisabledState);
                buildSkinnedOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState);
                buildSkinnedOitppllMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
                buildSkinnedOitppllMeshPSO->SetAS(gShaders[L"CullAS"].get());
                buildSkinnedOitppllMeshPSO->SetMS(gShaders[L"OpaqueSkinnedMS"].get());
                buildSkinnedOitppllMeshPSO->SetPS(gShaders[L"BuildOitppllPS"].get());
                buildSkinnedOitppllMeshPSO->Finalize(device);
                
                gPSOs[L"BuildSkinnedOitppll"] = std::move(buildSkinnedOitppllMeshPSO);
            }

            if (gPSOs.count(L"DrawOitppll") == 0)
            {
                auto drawOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
                drawOitppllMeshPSO->SetRootSignature(sRootSignature.get());
                drawOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState);
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

        virtual void InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)
        {
            mHiZMipLevels = fmax(ceilf(log2f(mWidth)), ceilf(log2f(mHeight)));

            D3D12_CLEAR_VALUE frameOptClearValue = CD3DX12_CLEAR_VALUE(mFrameFormat, DirectX::Colors::Gray);
            mFrameMap = make_unique<ColorBuffer>(
                mWidth,
                mHeight,
                1,
                COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
                mFrameFormat,
                device,
                heap,
                descriptorManager,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                &frameOptClearValue);

            D3D12_CLEAR_VALUE depthStencilOptClearValue = CD3DX12_CLEAR_VALUE(GetDsvFormat(mDepthStencilFormat), 1.f, 0);
            mDepthStencilMap = make_unique<ColorBuffer>(
                mWidth,
                mHeight,
                1,
                COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
                mDepthStencilFormat,
                device,
                heap,
                descriptorManager,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
                &depthStencilOptClearValue);

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
                mHiZMipLevels);

            mOitppllBuffer = make_unique<StructuredBuffer>(
                mWidth * mHeight * 4,
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

        void DrawOpaque(ID3D12GraphicsCommandList* cmdList)
        {
            cmdList->SetPipelineState(gPSOs[L"OpaqueStatic"]->Get());
            ExecuteIndirect(cmdList, GetIndirectCommandBuffer(OPAQUE_STATIC));

            cmdList->SetPipelineState(gPSOs[L"OpaqueSkinned"]->Get());
            ExecuteIndirect(cmdList, GetIndirectCommandBuffer(OPAQUE_SKINNED));
            
            DrawSkyBox(cmdList, mScene->GetSkyBox()->GetMeshCBAddress());
        }

        void DrawTransparent(ID3D12GraphicsCommandList* cmdList)
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
            
            cmdList->SetPipelineState(gPSOs[L"BuildStaticOitppll"]->Get());
            ExecuteIndirect(cmdList, GetIndirectCommandBuffer(TRANSPARENT_STATIC));
            
            cmdList->SetPipelineState(gPSOs[L"BuildSkinnedOitppll"]->Get());
            ExecuteIndirect(cmdList, GetIndirectCommandBuffer(TRANSPARENT_SKINNED));

            mOitppllBuffer->Transition(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            mStartOffsetBuffer->Transition(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            cmdList->OMSetRenderTargets(1, GetRvaluePtr(GetFrameRtv()), true, nullptr);
            cmdList->SetPipelineState(gPSOs[L"DrawOitppll"]->Get());
            static_cast<ID3D12GraphicsCommandList6*>(cmdList)->DispatchMesh(1, 1, 1);
        }

        void DrawSkyBox(ID3D12GraphicsCommandList* cmdList, D3D12_GPU_VIRTUAL_ADDRESS skyBoxMeshCBAddr)
        {
            cmdList->SetPipelineState(gPSOs[L"SkyBox"]->Get());
            cmdList->SetGraphicsRootConstantBufferView(MESH_CB, skyBoxMeshCBAddr);
            static_cast<ID3D12GraphicsCommandList6*>(cmdList)->DispatchMesh(1, 1, 1);
        }

        void Clear(ID3D12GraphicsCommandList* cmdList)
        {
            mScene->ClearCullMark(cmdList);

            for (int i = 0; i < MESH_TYPE_COUNT; ++i)
            {
                MeshType type = MeshType(i);

                mCulledCommandBuffer[type]->Transition(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);
                mCulledCommandBuffer[type]->ResetCounter(cmdList);
                mCulledCommandBuffer[type]->Transition(cmdList, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
            }
        }

        void CullInstances(ID3D12GraphicsCommandList* cmdList, bool hist, bool opaque)
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
                
                mCullIdx[type][CULL_HIST] = hist;
                uint32_t count = ceilf(mCullIdx[type][CULL_MESH_COUNT] / 32.f);

                cmdList->SetComputeRoot32BitConstants(PASS_CONSTANTS, CULL_IDX_COUNT, mCullIdx[type].data(), 0);
                mCulledCommandBuffer[type]->Transition(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                cmdList->Dispatch(count, 1, 1);
                mCulledCommandBuffer[type]->UavBarrier(cmdList);
            }
        }

        void CullMeshlets(ID3D12GraphicsCommandList* cmdList, bool hist, bool opaque)
        {
            ID3D12PipelineState* pso[] = {
                gPSOs[L"OpaqueStaticMeshletCull"]->Get(),
                gPSOs[L"OpaqueSkinnedMeshletCull"]->Get(),
                gPSOs[L"TransparentMeshletCull"]->Get(),
                gPSOs[L"TransparentMeshletCull"]->Get() };

            uint32_t start = opaque ? OPAQUE_MESH_START : TRANSPARENT_MESH_START;
            uint32_t end = start + (opaque ? OPAQUE_MESH_TYPE_COUNT : TRANSPARENT_MESH_TYPE_COUNT);

            if (opaque)
            {
                cmdList->ClearDepthStencilView(GetFrameDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
                cmdList->OMSetRenderTargets(0, nullptr, true, GetRvaluePtr(GetFrameDsv()));
            }

            for (int i = start; i < end; ++i)
            {
                MeshType type = MeshType(i);

                if (mCullIdx[type][CULL_MESH_COUNT] == 0)
                {
                    continue;
                }

                mCullIdx[type][CULL_HIST] = hist;

                cmdList->SetPipelineState(pso[i]);
                mCulledCommandBuffer[type]->Transition(cmdList, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
                cmdList->SetGraphicsRoot32BitConstants(PASS_CONSTANTS, CULL_IDX_COUNT, mCullIdx[type].data(), 0);
                ExecuteIndirect(cmdList, mCulledCommandBuffer[type].get());
            }
        }

        void GenerateHiZ(ID3D12GraphicsCommandList* cmdList)
        {
            cmdList->SetPipelineState(gPSOs[L"HiZGenerate"]->Get());

            for (int i = 0; i < mHiZMipLevels - 1; i += 5)
            {
                mHiZIdx[HIZ_SRC_MIP] = i;
                mHiZIdx[HIZ_NUM_MIP_LEVEL] = i + 5 >= mHiZMipLevels ? mHiZMipLevels - i - 1 : 5;
                cmdList->SetComputeRoot32BitConstants(PASS_CONSTANTS, mHiZIdx.size(), mHiZIdx.data(), 0);
                
                uint32_t width = ceilf((mWidth >> i) / 32.f);
                uint32_t height = ceilf((mHeight >> i) / 32.f);
                cmdList->Dispatch(width, height, 1);
                mHiZMap->UavBarrier(cmdList);
            }
        }

        enum
        {
            HIZ_DEPTH_IDX,
            HIZ_R_IDX,
            HIZ_W_IDX,
            HIZ_SRC_MIP,
            HIZ_NUM_MIP_LEVEL,
            HIZ_IDX_COUNT
        };

        enum
        {
            CULL_CULLED_COMMAND_BUFFER_IDX,
            CULL_MESH_COUNT,
            CULL_MESH_OFFSET,
            CULL_HIZ_IDX,
            CULL_HIST,
            CULL_IDX_COUNT
        };

        Scene *mScene;

        unique_ptr<ColorBuffer> mFrameMap;
        unique_ptr<ColorBuffer> mDepthStencilMap;
        unique_ptr<ColorBuffer> mHiZMap;

        unique_ptr<StructuredBuffer> mOitppllBuffer;
        unique_ptr<RawBuffer> mStartOffsetBuffer;
        unique_ptr<RawBuffer> mCounterBuffer;

        vector<unique_ptr<StructuredBuffer>> mCulledCommandBuffer;
        unique_ptr<StructuredBufferPool> mCulledCommandBufferPool;

        vector<vector<uint32_t>> mCullIdx;
        vector<uint32_t> mHiZIdx;
        uint32_t mHiZMipLevels;

        DXGI_FORMAT mFrameFormat;
        DXGI_FORMAT mDepthStencilFormat;
        DXGI_FORMAT mHiZFormat;
    };
}