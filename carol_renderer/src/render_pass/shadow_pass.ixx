export module carol.renderer.render_pass.shadow_pass;
import carol.renderer.render_pass.render_pass;
import carol.renderer.dx12;
import carol.renderer.scene;
import carol.renderer.utils;
import <DirectXMath.h>;
import <DirectXColors.h>;
import <vector>;
import <memory>;
import <string_view>;

namespace Carol
{
    using std::make_unique;
    using std::unique_ptr;
    using std::vector;
    using std::wstring;
    using std::wstring_view;
    using namespace DirectX;

    export class ShadowPass : public RenderPass
    {
    public:
        ShadowPass(
            Light light,
            uint32_t width = 1024,
            uint32_t height = 1024,
            uint32_t depthBias = 60000,
            float depthBiasClamp = 0.01f,
            float slopeScaledDepthBias = 4.f,
            DXGI_FORMAT shadowFormat = DXGI_FORMAT_R32_FLOAT,
            DXGI_FORMAT hiZFormat = DXGI_FORMAT_R32_FLOAT)
            : mLight(make_unique<Light>(light)),
              mDepthBias(depthBias),
              mDepthBiasClamp(depthBiasClamp),
              mSlopeScaledDepthBias(slopeScaledDepthBias),
              mCullIdx(OPAQUE_MESH_TYPE_COUNT),
              mHiZIdx(HIZ_IDX_COUNT),
              mHiZMipLevels(fmax(ceilf(log2f(width)), ceilf(log2f(height)))),
              mCulledCommandBuffer(gNumFrame),
              mShadowFormat(shadowFormat),
              mHiZFormat(hiZFormat)
        {
            InitLightView();
            InitCamera();
            InitShaders();
            InitPSOs();
            OnResize(width, height);
        }

        virtual void Draw() override
        {
            Clear();
            gCommandList->RSSetViewports(1, &mViewport);
            gCommandList->RSSetScissorRects(1, &mScissorRect);

            GenerateHiZ();
            CullInstances(true);
            DrawShadow(true);

            GenerateHiZ();
            CullInstances(false);
            DrawShadow(false);
        }

        virtual void Update() override
        {
            XMMATRIX view = mCamera->GetView();
            XMMATRIX proj = mCamera->GetProj();
            XMMATRIX viewProj = XMMatrixMultiply(view, proj);
            static XMMATRIX tex(
                0.5f, 0.0f, 0.0f, 0.0f,
                0.0f, -0.5f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.5f, 0.5f, 0.0f, 1.0f);

            XMStoreFloat4x4(&mLight->View, XMMatrixTranspose(view));
            XMStoreFloat4x4(&mLight->Proj, XMMatrixTranspose(proj));
            XMStoreFloat4x4(&mLight->ViewProj, XMMatrixTranspose(viewProj));
            XMStoreFloat4x4(&mLight->ViewProjTex, XMMatrixTranspose(XMMatrixMultiply(viewProj, tex)));

            for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
            {
                MeshType type = MeshType(OPAQUE_MESH_START + i);
                TestCommandBufferSize(mCulledCommandBuffer[gCurrFrame][type], gScene->GetMeshesCount(type));

                mCullIdx[i][CULL_CULLED_COMMAND_BUFFER_IDX] = mCulledCommandBuffer[gCurrFrame][i]->GetGpuUavIdx();
                mCullIdx[i][CULL_MESH_COUNT] = gScene->GetMeshesCount(type);
                mCullIdx[i][CULL_MESH_OFFSET] = gScene->GetMeshCBStartOffet(type);
                mCullIdx[i][CULL_HIZ_IDX] = mHiZMap->GetGpuSrvIdx();
                mCullIdx[i][CULL_LIGHT_IDX] = 0;
            }

            mHiZIdx[HIZ_DEPTH_IDX] = mShadowMap->GetGpuSrvIdx();
            mHiZIdx[HIZ_R_IDX] = mHiZMap->GetGpuSrvIdx();
            mHiZIdx[HIZ_W_IDX] = mHiZMap->GetGpuUavIdx();
        }

        virtual void ReleaseIntermediateBuffers() override
        {
        }

        uint32_t GetShadowSrvIdx()
        {
            return mShadowMap->GetGpuSrvIdx();
        }

        const Light &GetLight()
        {
            return *mLight;
        }

    protected:
        virtual void InitShaders() override
        {
            vector<wstring_view> nullDefines{};

            vector<wstring_view> shadowDefines =
                {
                    L"SHADOW=1"};

            vector<wstring_view> skinnedShadowDefines =
                {
                    L"SKINNED=1",
                    L"SHADOW=1"};

            vector<wstring_view> shadowCullDefines =
                {
                    L"SHADOW=1",
                    L"OCCLUSION=1",
                    L"WRITE=1"};

            gShaders[L"ShadowCullCS"] = make_unique<Shader>(L"shader\\cull_cs.hlsl", shadowCullDefines, L"main", L"cs_6_6");
            gShaders[L"ShadowAS"] = make_unique<Shader>(L"shader\\cull_as.hlsl", shadowCullDefines, L"main", L"as_6_6");
            gShaders[L"ShadowStaticMS"] = make_unique<Shader>(L"shader\\depth_ms.hlsl", shadowDefines, L"main", L"ms_6_6");
            gShaders[L"ShadowSkinnedMS"] = make_unique<Shader>(L"shader\\depth_ms.hlsl", skinnedShadowDefines, L"main", L"ms_6_6");
        }

        virtual void InitPSOs() override
        {
            auto shadowStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
            shadowStaticMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
            shadowStaticMeshPSO->SetDepthTargetFormat(GetDsvFormat(mShadowFormat));
            shadowStaticMeshPSO->SetAS(gShaders[L"ShadowAS"].get());
            shadowStaticMeshPSO->SetMS(gShaders[L"ShadowStaticMS"].get());
            shadowStaticMeshPSO->Finalize();
            gPSOs[L"ShadowStatic"] = move(shadowStaticMeshPSO);

            auto shadowSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
            shadowSkinnedMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
            shadowSkinnedMeshPSO->SetDepthTargetFormat(GetDsvFormat(mShadowFormat));
            shadowSkinnedMeshPSO->SetAS(gShaders[L"ShadowAS"].get());
            shadowSkinnedMeshPSO->SetMS(gShaders[L"ShadowSkinnedMS"].get());
            shadowSkinnedMeshPSO->Finalize();
            gPSOs[L"ShadowSkinned"] = move(shadowSkinnedMeshPSO);

            auto shadowInstanceCullComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
            shadowInstanceCullComputePSO->SetCS(gShaders[L"ShadowCullCS"].get());
            shadowInstanceCullComputePSO->Finalize();
            gPSOs[L"ShadowInstanceCull"] = move(shadowInstanceCullComputePSO);
        }

        virtual void InitBuffers() override
        {
            D3D12_CLEAR_VALUE optClearValue;
            optClearValue.Format = GetDsvFormat(mShadowFormat);
            optClearValue.DepthStencil.Depth = 1.0f;
            optClearValue.DepthStencil.Stencil = 0;

            mShadowMap = make_unique<ColorBuffer>(
                mWidth,
                mHeight,
                1,
                COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
                mShadowFormat,
                gHeapManager->GetDefaultBuffersHeap(),
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
                &optClearValue);

            mHiZMap = make_unique<ColorBuffer>(
                mWidth,
                mHeight,
                1,
                COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
                mHiZFormat,
                gHeapManager->GetDefaultBuffersHeap(),
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                nullptr,
                mHiZMipLevels);

            for (int i = 0; i < gNumFrame; ++i)
            {
                mCulledCommandBuffer[i].resize(OPAQUE_MESH_TYPE_COUNT);

                for (int j = 0; j < OPAQUE_MESH_TYPE_COUNT; ++j)
                {
                    ResizeCommandBuffer(mCulledCommandBuffer[i][j], 1024, sizeof(IndirectCommand));
                }
            }

            for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
            {
                mCullIdx[i].resize(CULL_IDX_COUNT);
            }
        }

        void InitLightView()
        {
            mViewport.TopLeftX = 0.0f;
            mViewport.TopLeftY = 0.0f;
            mViewport.Width = mWidth;
            mViewport.Height = mHeight;
            mViewport.MinDepth = 0.0f;
            mViewport.MaxDepth = 1.0f;
            mScissorRect = {0, 0, (int)mWidth, (int)mHeight};
        }

        void InitCamera()
        {
            mLight->Position = XMFLOAT3(-mLight->Direction.x * 140.f, -mLight->Direction.y * 140.f, -mLight->Direction.z * 140.f);
            mCamera = make_unique<OrthographicCamera>(mLight->Direction, XMFLOAT3{0.0f, 0.0f, 0.0f}, 70.0f);
        }

        void Clear()
        {
            gScene->ClearCullMark();

            for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
            {
                gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[gCurrFrame][i]->Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST)));
                mCulledCommandBuffer[gCurrFrame][i]->ResetCounter();
                gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[gCurrFrame][i]->Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)));
            }
        }

        void CullInstances(bool hist)
        {
            gCommandList->SetPipelineState(gPSOs[L"ShadowInstanceCull"]->Get());

            for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
            {
                if (mCullIdx[i][CULL_MESH_COUNT] == 0)
                {
                    continue;
                }

                mCullIdx[i][CULL_HIST] = hist;
                uint32_t count = ceilf(mCullIdx[i][CULL_MESH_COUNT] / 32.f);

                gCommandList->SetComputeRoot32BitConstants(PASS_CONSTANTS, CULL_IDX_COUNT, mCullIdx[i].data(), 0);
                gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[gCurrFrame][i]->Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)));
                gCommandList->Dispatch(count, 1, 1);
                gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::UAV(mCulledCommandBuffer[gCurrFrame][i]->Get())));
            }
        }

        void DrawShadow(bool hist)
        {
            gCommandList->ClearDepthStencilView(mShadowMap->GetDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
            gCommandList->OMSetRenderTargets(0, nullptr, true, GetRvaluePtr(mShadowMap->GetDsv()));

            ID3D12PipelineState *pso[] = {gPSOs[L"ShadowStatic"]->Get(), gPSOs[L"ShadowSkinned"]->Get()};

            for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
            {
                if (mCullIdx[i][CULL_MESH_COUNT] == 0)
                {
                    continue;
                }

                mCullIdx[i][CULL_HIST] = hist;

                gCommandList->SetPipelineState(pso[i]);
                gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[gCurrFrame][i]->Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)));
                gCommandList->SetGraphicsRoot32BitConstants(PASS_CONSTANTS, CULL_IDX_COUNT, mCullIdx[i].data(), 0);
                gScene->ExecuteIndirect(mCulledCommandBuffer[gCurrFrame][i].get());
            }
        }

        void GenerateHiZ()
        {
            gCommandList->SetPipelineState(gPSOs[L"HiZGenerate"]->Get());

            for (int i = 0; i < mHiZMipLevels - 1; i += 5)
            {
                mHiZIdx[HIZ_SRC_MIP] = i;
                mHiZIdx[HIZ_NUM_MIP_LEVEL] = i + 5 >= mHiZMipLevels ? mHiZMipLevels - i - 1 : 5;
                gCommandList->SetComputeRoot32BitConstants(PASS_CONSTANTS, HIZ_IDX_COUNT, mHiZIdx.data(), 0);

                uint32_t width = ceilf((mWidth >> i) / 32.f);
                uint32_t height = ceilf((mHeight >> i) / 32.f);
                gCommandList->Dispatch(width, height, 1);
                gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::UAV(mHiZMap->Get())));
            }
        }

        void TestCommandBufferSize(unique_ptr<StructuredBuffer> &buffer, uint32_t numElements)
        {
            if (buffer->GetNumElements() < numElements)
            {
                ResizeCommandBuffer(buffer, numElements, buffer->GetElementSize());
            }
        }

        void ResizeCommandBuffer(unique_ptr<StructuredBuffer> &buffer, uint32_t numElements, uint32_t elementSize)
        {
            buffer = make_unique<StructuredBuffer>(
                numElements,
                elementSize,
                gHeapManager->GetDefaultBuffersHeap(),
                D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
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
            CULL_LIGHT_IDX,
            CULL_IDX_COUNT
        };

        unique_ptr<Light> mLight;
        unique_ptr<ColorBuffer> mShadowMap;
        unique_ptr<ColorBuffer> mHiZMap;
        vector<vector<unique_ptr<StructuredBuffer>>> mCulledCommandBuffer;

        uint32_t mDepthBias;
        float mDepthBiasClamp;
        float mSlopeScaledDepthBias;

        vector<vector<uint32_t>> mCullIdx;
        vector<uint32_t> mHiZIdx;
        uint32_t mHiZMipLevels;

        unique_ptr<Camera> mCamera;

        DXGI_FORMAT mShadowFormat;
        DXGI_FORMAT mHiZFormat;
    };
}