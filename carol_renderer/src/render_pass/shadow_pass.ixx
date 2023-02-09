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
import <cmath>;

namespace Carol
{
    using std::make_unique;
    using std::unique_ptr;
    using std::vector;
    using std::wstring;
    using std::wstring_view;
    using namespace DirectX;

    class ShadowPass : public RenderPass
    {
    public:
        ShadowPass(
            ID3D12Device* device,
            Heap* heap,
            DescriptorManager* descriptorManager,
            Scene* scene,
            Light light,
            uint32_t width = 512,
            uint32_t height = 512,
            uint32_t depthBias = 60000,
            float depthBiasClamp = 0.01f,
            float slopeScaledDepthBias = 4.f,
            DXGI_FORMAT shadowFormat = DXGI_FORMAT_R32_FLOAT,
            DXGI_FORMAT hiZFormat = DXGI_FORMAT_R32_FLOAT)
            : mLight(make_unique<Light>(light)),
              mScene(scene),
              mDepthBias(depthBias),
              mDepthBiasClamp(depthBiasClamp),
              mSlopeScaledDepthBias(slopeScaledDepthBias),
              mCullIdx(OPAQUE_MESH_TYPE_COUNT),
              mHiZIdx(HIZ_IDX_COUNT),
              mHiZMipLevels(fmax(ceilf(log2f(width)), ceilf(log2f(height)))),
              mCulledCommandBuffer(OPAQUE_MESH_TYPE_COUNT),
              mShadowFormat(shadowFormat),
              mHiZFormat(hiZFormat)
        {
            InitLightView();
            InitShaders();
            InitPSOs(device);
            OnResize(
                width,
                height,
                device,
                heap,
                descriptorManager);
        }

        virtual void Draw(ID3D12GraphicsCommandList* cmdList) override
        {
            Clear(cmdList);
            cmdList->RSSetViewports(1, &mViewport);
            cmdList->RSSetScissorRects(1, &mScissorRect);

            GenerateHiZ(cmdList);
            CullInstances(true, cmdList);
            DrawShadow(true, cmdList);

            GenerateHiZ(cmdList);
            CullInstances(false, cmdList);
            DrawShadow(false, cmdList);
        }

        void Update(uint32_t lightIdx, uint64_t cpuFenceValue, uint64_t completedFenceValue)
        {
            XMMATRIX view = mCamera->GetView();
            XMMATRIX proj = mCamera->GetProj();
            XMMATRIX viewProj = XMMatrixMultiply(view, proj);
            
            XMStoreFloat4x4(&mLight->View, XMMatrixTranspose(view));
            XMStoreFloat4x4(&mLight->Proj, XMMatrixTranspose(proj));
            XMStoreFloat4x4(&mLight->ViewProj, XMMatrixTranspose(viewProj));

            for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
            {
                MeshType type = MeshType(OPAQUE_MESH_START + i);
                mCulledCommandBufferPool->DiscardBuffer(mCulledCommandBuffer[type].release(), cpuFenceValue);
                mCulledCommandBuffer[type] = mCulledCommandBufferPool->RequestBuffer(completedFenceValue, mScene->GetMeshesCount(type));

                mCullIdx[i][CULL_CULLED_COMMAND_BUFFER_IDX] = mCulledCommandBuffer[type]->GetGpuUavIdx();
                mCullIdx[i][CULL_MESH_COUNT] = mScene->GetMeshesCount(type);
                mCullIdx[i][CULL_MESH_OFFSET] = mScene->GetMeshCBStartOffet(type);
                mCullIdx[i][CULL_HIZ_IDX] = mHiZMap->GetGpuSrvIdx();
                mCullIdx[i][CULL_LIGHT_IDX] = lightIdx;
            }

            mHiZIdx[HIZ_DEPTH_IDX] = mShadowMap->GetGpuSrvIdx();
            mHiZIdx[HIZ_R_IDX] = mHiZMap->GetGpuSrvIdx();
            mHiZIdx[HIZ_W_IDX] = mHiZMap->GetGpuUavIdx();
        }
 
        uint32_t GetShadowSrvIdx()const
        {
            return mShadowMap->GetGpuSrvIdx();
        }

        const Light &GetLight()const
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

            if (gShaders.count(L"ShadowCullCS") == 0)
            {
                gShaders[L"ShadowCullCS"] = make_unique<Shader>(L"shader\\cull_cs.hlsl", shadowCullDefines, L"main", L"cs_6_6");
            }

            if (gShaders.count(L"ShadowAS") == 0)
            {
                gShaders[L"ShadowAS"] = make_unique<Shader>(L"shader\\cull_as.hlsl", shadowCullDefines, L"main", L"as_6_6");
            }
            
            if (gShaders.count(L"ShadowStaticMS") == 0)
            {
                gShaders[L"ShadowStaticMS"] = make_unique<Shader>(L"shader\\depth_ms.hlsl", shadowDefines, L"main", L"ms_6_6");
            }

            if (gShaders.count(L"ShadowSkinnedMS") == 0)
            {
                gShaders[L"ShadowSkinnedMS"] = make_unique<Shader>(L"shader\\depth_ms.hlsl", skinnedShadowDefines, L"main", L"ms_6_6");
            }
        }

        virtual void InitPSOs(ID3D12Device* device) override
        {
            if (gPSOs.count(L"ShadowStatic") == 0)
            {
                auto shadowStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
                shadowStaticMeshPSO->SetRootSignature(sRootSignature.get());
                shadowStaticMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
                shadowStaticMeshPSO->SetDepthTargetFormat(GetDsvFormat(mShadowFormat));
                shadowStaticMeshPSO->SetAS(gShaders[L"ShadowAS"].get());
                shadowStaticMeshPSO->SetMS(gShaders[L"ShadowStaticMS"].get());
                shadowStaticMeshPSO->Finalize(device);
            
                gPSOs[L"ShadowStatic"] = std::move(shadowStaticMeshPSO);
            }

            if (gPSOs.count(L"ShadowSkinned") == 0)
            {
                auto shadowSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
                shadowSkinnedMeshPSO->SetRootSignature(sRootSignature.get());
                shadowSkinnedMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
                shadowSkinnedMeshPSO->SetDepthTargetFormat(GetDsvFormat(mShadowFormat));
                shadowSkinnedMeshPSO->SetAS(gShaders[L"ShadowAS"].get());
                shadowSkinnedMeshPSO->SetMS(gShaders[L"ShadowSkinnedMS"].get());
                shadowSkinnedMeshPSO->Finalize(device);
            
                gPSOs[L"ShadowSkinned"] = std::move(shadowSkinnedMeshPSO);
            }

            if (gPSOs.count(L"ShadowInstanceCull") == 0)
            {
                auto shadowInstanceCullComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
                shadowInstanceCullComputePSO->SetRootSignature(sRootSignature.get());
                shadowInstanceCullComputePSO->SetCS(gShaders[L"ShadowCullCS"].get());
                shadowInstanceCullComputePSO->Finalize(device);

                gPSOs[L"ShadowInstanceCull"] = std::move(shadowInstanceCullComputePSO);
            }
        }

        virtual void InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)override
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
                device,
                heap,
                descriptorManager,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
                &optClearValue);

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

            mCulledCommandBufferPool = make_unique<StructuredBufferPool>(
                1024,
                sizeof(IndirectCommand),
                device,
                heap,
                descriptorManager,
                D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

            for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
            {
                MeshType type = MeshType(OPAQUE_MESH_START + i);
                mCullIdx[type].resize(CULL_IDX_COUNT);
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

        virtual void InitCamera() = 0;

        void Clear(ID3D12GraphicsCommandList* cmdList)
        {
            mScene->ClearCullMark(cmdList);

            for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
            {
                MeshType type = MeshType(OPAQUE_MESH_START + i);

                mCulledCommandBuffer[type]->Transition(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);
                mCulledCommandBuffer[type]->ResetCounter(cmdList);
                mCulledCommandBuffer[type]->Transition(cmdList, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
            }
        }

        void CullInstances(bool hist, ID3D12GraphicsCommandList* cmdList)
        {
            cmdList->SetPipelineState(gPSOs[L"ShadowInstanceCull"]->Get());
	
            for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
            {
                MeshType type = MeshType(OPAQUE_MESH_START + i);

                if (mCullIdx[type][CULL_MESH_COUNT] == 0)
                {
                    continue;
                }
                
                mCullIdx[i][CULL_HIST] = hist;
                uint32_t count = ceilf(mCullIdx[type][CULL_MESH_COUNT] / 32.f);

                cmdList->SetComputeRoot32BitConstants(PASS_CONSTANTS, CULL_IDX_COUNT, mCullIdx[i].data(), 0);
                mCulledCommandBuffer[type]->Transition(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                cmdList->Dispatch(count, 1, 1);
                mCulledCommandBuffer[type]->UavBarrier(cmdList);
            }
        }

        void DrawShadow(bool hist, ID3D12GraphicsCommandList* cmdList)
        {
            cmdList->ClearDepthStencilView(mShadowMap->GetDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
            cmdList->OMSetRenderTargets(0, nullptr, true, GetRvaluePtr(mShadowMap->GetDsv()));

            ID3D12PipelineState* pso[] = {gPSOs[L"ShadowStatic"]->Get(), gPSOs[L"ShadowSkinned"]->Get()};

            for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
            {
                MeshType type = MeshType(OPAQUE_MESH_START + i);

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
                cmdList->SetComputeRoot32BitConstants(PASS_CONSTANTS, HIZ_IDX_COUNT, mHiZIdx.data(), 0);
                
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
            CULL_LIGHT_IDX,
            CULL_IDX_COUNT
        };

        unique_ptr<Light> mLight;
        unique_ptr<ColorBuffer> mShadowMap;
        unique_ptr<ColorBuffer> mHiZMap;

        vector<unique_ptr<StructuredBuffer>> mCulledCommandBuffer;
        unique_ptr<StructuredBufferPool> mCulledCommandBufferPool;

        Scene *mScene;

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

    class DirectLightShadowPass : public ShadowPass
    {
    public:
		DirectLightShadowPass(
            ID3D12Device* device,
            Heap* heap,
            DescriptorManager* descriptorManager,
            Scene* scene,
            Light light,
            uint32_t width = 512,
            uint32_t height = 512,
            uint32_t depthBias = 60000,
            float depthBiasClamp = 0.01f,
            float slopeScaledDepthBias = 4.f,
            DXGI_FORMAT shadowFormat = DXGI_FORMAT_R32_FLOAT,
            DXGI_FORMAT hiZFormat = DXGI_FORMAT_R32_FLOAT)
            :ShadowPass(
                device,
                heap,
                descriptorManager,
                scene,
                light, 
                width, 
                height, 
                depthBias, 
                depthBiasClamp, 
                slopeScaledDepthBias, 
                shadowFormat, 
                hiZFormat)
        {
            InitCamera();
        }

        void Update(uint32_t lightIdx, const PerspectiveCamera* camera, float zn, float zf, uint64_t cpuFenceValue, uint64_t completedFenceValue)
        {
            static float dx[4] = { -1.f,1.f,-1.f,1.f };
            static float dy[4] = { -1.f,-1.f,1.f,1.f };

            XMFLOAT4 pointNear;
            pointNear.z = zn;
            pointNear.y = zn * tanf(0.5f * camera->GetFovY());
            pointNear.x = zn * tanf(0.5f * camera->GetFovX());
            
            XMFLOAT4 pointFar;
            pointFar.z = zf;
            pointFar.y = zf * tanf(0.5f * camera->GetFovY());
            pointFar.x = zf * tanf(0.5f * camera->GetFovX());

            vector<XMFLOAT4> frustumSliceExtremaPoints;
            XMMATRIX perspView = camera->GetView();
            XMMATRIX invPerspView = XMMatrixInverse(GetRvaluePtr(XMMatrixDeterminant(perspView)), perspView);
            XMMATRIX orthoView = mCamera->GetView();
            XMMATRIX invOrthoView = XMMatrixInverse(GetRvaluePtr(XMMatrixDeterminant(orthoView)), orthoView);
            XMMATRIX invPerspOrthoView = XMMatrixMultiply(invPerspView, orthoView);

            for (int i = 0; i < 4; ++i)
            {
                XMFLOAT4 point;
                
                point = { pointNear.x* dx[i], pointNear.y* dy[i], pointNear.z, 1.f };
                XMStoreFloat4(&point, XMVector4Transform(XMLoadFloat4(&point), invPerspOrthoView));
                frustumSliceExtremaPoints.push_back(point);

                point = { pointFar.x* dx[i], pointFar.y* dy[i], pointFar.z, 1.f };
                XMStoreFloat4(&point, XMVector4Transform(XMLoadFloat4(&point), invPerspOrthoView));
                frustumSliceExtremaPoints.push_back(point);
            }

            XMFLOAT4 boxMin = { D3D12_FLOAT32_MAX,D3D12_FLOAT32_MAX,D3D12_FLOAT32_MAX,1.f };
            XMFLOAT4 boxMax = { -D3D12_FLOAT32_MAX,-D3D12_FLOAT32_MAX,-D3D12_FLOAT32_MAX,1.f };

            for (auto& point : frustumSliceExtremaPoints)
            {
                boxMin.x = fmin(boxMin.x, point.x);
                boxMin.y = fmin(boxMin.y, point.y);
                boxMin.z = fmin(boxMin.z, point.z);

                boxMax.x = fmax(boxMax.x, point.x);
                boxMax.y = fmax(boxMax.y, point.y);
                boxMax.z = fmax(boxMax.z, point.z);
            }
            
            XMFLOAT4 center;
            XMStoreFloat4(&center, XMVector4Transform(0.5f * (XMLoadFloat4(&boxMin) + XMLoadFloat4(&boxMax)), invOrthoView));

            dynamic_cast<OrthographicCamera*>(mCamera.get())->SetLens(boxMax.x - boxMin.x, boxMax.y - boxMin.y, 1.f, boxMax.z + boxMax.z - boxMin.z);
            mCamera->LookAt(XMLoadFloat4(&center) - 0.5f * (boxMax.z - boxMin.z) * XMLoadFloat3(&mLight->Direction), XMLoadFloat4(&center), { 0.f,1.f,0.f,0.f });
            mCamera->UpdateViewMatrix();

            ShadowPass::Update(lightIdx, cpuFenceValue, completedFenceValue);
        }

    protected:
        virtual void InitCamera()override
        {
        	mLight->Position = XMFLOAT3(-mLight->Direction.x * 140.f, -mLight->Direction.y * 140.f, -mLight->Direction.z * 140.f);
            mCamera = make_unique<OrthographicCamera>(70, 0, 280);
            mCamera->LookAt(mLight->Position, { 0.f,0.f,0.f }, { 0.f,1.f,0.f });
            mCamera->UpdateViewMatrix();
        }
    };

    export class CascadedShadowPass : public RenderPass
    {
	public:
        CascadedShadowPass(
            ID3D12Device* device,
            Heap* heap,
            DescriptorManager* descriptorManager,
            Scene* scene,
            Light light,
            uint32_t splitLevel = 5,
            uint32_t width = 512,
            uint32_t height = 512,
            uint32_t depthBias = 60000,
            float depthBiasClamp = 0.01f,
            float slopeScaledDepthBias = 4.f,
            DXGI_FORMAT shadowFormat = DXGI_FORMAT_R32_FLOAT,
            DXGI_FORMAT hiZFormat = DXGI_FORMAT_R32_FLOAT)
            :mSplitLevel(splitLevel),
            mSplitZ(splitLevel + 1),
            mShadow(splitLevel)
        {
            for (auto& shadow : mShadow)
            {
                shadow = make_unique<DirectLightShadowPass>(
                    device,
                    heap,
                    descriptorManager,
                    scene,
                    light,
                    width,
                    height,
                    depthBias,
                    depthBiasClamp,
                    slopeScaledDepthBias,
                    shadowFormat,
                    hiZFormat);
            }
        }

		virtual void Draw(ID3D12GraphicsCommandList* cmdList)override
        {
            for (auto& shadow : mShadow)
            {
                shadow->Draw(cmdList);
            }
        }

        void Update(const PerspectiveCamera* camera, uint64_t cpuFenceValue, uint64_t completedFenceValue, float logWeight = 0.5f, float bias = 0.f)
        {
            float nearZ = camera->GetNearZ();
            float farZ = camera->GetFarZ();

            for (int i = 0; i < mSplitLevel + 1; ++i)
            {
                mSplitZ[i] = logWeight * nearZ * pow(farZ / nearZ, 1.f * i / mSplitLevel) + (1 - logWeight) * (nearZ + (farZ - nearZ) * (1.f * i / mSplitLevel)) + bias;
            }

            for (int i = 0; i < mSplitLevel; ++i)
            {
                mShadow[i]->Update(i, camera, mSplitZ[i], mSplitZ[i + 1], cpuFenceValue, completedFenceValue);
            }
        }

        uint32_t GetSplitLevel()const
        {
            return mSplitLevel;
        }

        float GetSplitZ(uint32_t idx)const
        {
            return mSplitZ[idx];
        }
        
        uint32_t GetShadowSrvIdx(uint32_t idx)const
        {
            return mShadow[idx]->GetShadowSrvIdx();
        }

        const Light& GetLight(uint32_t idx)const
        {
            return mShadow[idx]->GetLight();
        }

	protected:
		virtual void InitShaders()override
        {

        }

        virtual void InitPSOs(ID3D12Device* device)override
        {

        }

		virtual void InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)override
        {

        }

        uint32_t mSplitLevel;
        std::vector<float> mSplitZ;
        std::vector<std::unique_ptr<DirectLightShadowPass>> mShadow;
    };
}