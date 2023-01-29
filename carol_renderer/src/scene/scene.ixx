export module carol.renderer.scene.scene;
import carol.renderer.scene.assimp;
import carol.renderer.scene.camera;
import carol.renderer.scene.light;
import carol.renderer.scene.assimp;
import carol.renderer.scene.model;
import carol.renderer.scene.mesh;
import carol.renderer.scene.scene_node;
import carol.renderer.scene.texture;
import carol.renderer.scene.timer;
import carol.renderer.dx12;
import carol.renderer.utils;
import <DirectXMath.h>;
import <DirectXCollision.h>;
import <vector>;
import <memory>;
import <unordered_map>;
import <string>;
import <string_view>;

#define WARP_SIZE 32

namespace Carol
{
    using std::make_unique;
    using std::unique_ptr;
    using std::unordered_map;
    using std::vector;
    using std::wstring;
    using std::wstring_view;
    using namespace DirectX;
    using Microsoft::WRL::ComPtr;

    export class Scene
    {
    public:
        Scene(wstring_view name)
        	:mRootNode(make_unique<SceneNode>()),
            mMeshes(MESH_TYPE_COUNT),
            mIndirectCommandBuffer(gNumFrame),
            mMeshCB(gNumFrame),
            mSkinnedCB(gNumFrame),
            mInstanceFrustumCulledMarkBuffer(make_unique<RawBuffer>(
            2 << 16,
            gHeapManager->GetDefaultBuffersHeap(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)),
            mInstanceOcclusionPassedMarkBuffer(make_unique<RawBuffer>(
            2 << 16,
            gHeapManager->GetDefaultBuffersHeap(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
        {
            mRootNode->Name = name;
            InitBuffers();
        }

        Scene(const Scene &) = delete;
        Scene(Scene &&) = delete;
        Scene &operator=(const Scene &) = delete;

        vector<wstring_view> GetAnimationClips(wstring_view modelName)
        {
            return mModels[modelName.data()]->GetAnimationClips();
        }

        vector<wstring_view> GetModelNames()
        {
            vector<wstring_view> models;
            for (auto& [name, model] : mModels)
            {
                models.push_back(wstring_view(name.c_str(), name.size()));
            }

            return models;
        }

        bool IsAnyTransparentMeshes()
        {
            return mMeshes[TRANSPARENT_STATIC].size() + mMeshes[TRANSPARENT_SKINNED].size();
        }

        void LoadModel(wstring_view name, wstring_view path, wstring_view textureDir, bool isSkinned)
        {
            mRootNode->Children.push_back(make_unique<SceneNode>());
            auto& node = mRootNode->Children.back();
            node->Children.push_back(make_unique<SceneNode>());

            node->Name = name;
            mModels[node->Name] = make_unique<AssimpModel>(
                node->Children[0].get(),
                path,
                textureDir,
                isSkinned);

            for (auto& [name, mesh] : mModels[node->Name]->GetMeshes())
            {
                wstring meshName = node->Name + L'_' + name;
                uint32_t type = mesh->IsSkinned() | (mesh->IsTransparent() << 1);
                mMeshes[type][meshName] = mesh.get();
            }
        }

        void LoadGround()
        {
            mModels[L"Ground"] = make_unique<Model>();
            mModels[L"Ground"]->LoadGround();
            
            mRootNode->Children.push_back(make_unique<SceneNode>());
            auto& node = mRootNode->Children.back();

            node->Name = L"Ground";
            node->Children.push_back(make_unique<SceneNode>());
            node->Children[0]->Meshes.push_back(mModels[L"Ground"]->GetMesh(L"Ground"));

            for (auto& [name, mesh] : mModels[L"Ground"]->GetMeshes())
            {
                wstring meshName = L"Ground_" + name;
                mMeshes[OPAQUE_STATIC][meshName] = mesh.get();
            }
        }

        void LoadSkyBox()
        {
            mSkyBox = make_unique<Model>();
            mSkyBox->LoadSkyBox();
        }

        void UnloadModel(wstring_view modelName)
        {
            for (auto itr = mRootNode->Children.begin(); itr != mRootNode->Children.end(); ++itr)
            {
                if ((*itr)->Name == modelName)
                {
                    mRootNode->Children.erase(itr);
                    break;
                }
            }

            wstring name(modelName);

            for (auto& [meshName, mesh] : mModels[name]->GetMeshes())
            {
                wstring modelMeshName = name + L"_" + meshName;
                uint32_t type = mesh->IsSkinned() | (mesh->IsTransparent() << 1);
                mMeshes[type].erase(modelMeshName);
            }

            mModels.erase(name);
        }

        void ReleaseIntermediateBuffers()
        {
            for (auto& [name, model] : mModels)
            {
                model->ReleaseIntermediateBuffers();
            }
        }

        void ReleaseIntermediateBuffers(wstring_view modelName)
        {
            mModels[modelName.data()]->ReleaseIntermediateBuffers();
        }

        const unordered_map<wstring, Mesh *> &GetMeshes(MeshType type)
        {
            return mMeshes[type];
        }

        uint32_t GetMeshesCount(MeshType type)
        {
            return mMeshes[type].size();
        }

        Mesh *GetSkyBox()
        {
            return mSkyBox->GetMesh(L"SkyBox");
        }

        const unordered_map<wstring, unique_ptr<Model>> &GetModels()
        {
            return mModels;
        }

        uint32_t GetModelsCount()
        {
            return mModels.size();
        }

        void SetWorld(wstring_view modelName, XMMATRIX world)
        {
            for (auto& node : mRootNode->Children)
            {
                if (node->Name == modelName)
                {
                    XMStoreFloat4x4(&node->Transformation, world);
                }
            }
        }

        void SetAnimationClip(wstring_view modelName, wstring_view clipName)
        {
            mModels[modelName.data()]->SetAnimationClip(clipName);
        }

        void Update(Camera *camera, Timer *timer)
        {
            for (auto& [name, model] : mModels)
            {
                model->Update(timer);
            }

            ProcessNode(mRootNode.get(), XMMatrixIdentity());

            uint32_t totalMeshCount = 0;

            for (int i = 0; i < MESH_TYPE_COUNT; ++i)
            {
                totalMeshCount += GetMeshesCount(MeshType(i));

                if (i == 0)
                {
                    mMeshStartOffset[i] = 0;
                }
                else
                {
                    mMeshStartOffset[i] = mMeshStartOffset[i - 1] + GetMeshesCount(MeshType(i - 1));
                }
            }

            TestBufferSize(mIndirectCommandBuffer[gCurrFrame], totalMeshCount);
            TestBufferSize(mMeshCB[gCurrFrame], totalMeshCount);
            TestBufferSize(mSkinnedCB[gCurrFrame], GetModelsCount());

            int modelIdx = 0;
            for (auto& [name, model] : GetModels())
            {
                if (model->IsSkinned())
                {
                    mSkinnedCB[gCurrFrame]->CopyElements(model->GetSkinnedConstants(), modelIdx);
                    model->SetSkinnedCBAddress(mSkinnedCB[gCurrFrame]->GetElementAddress(modelIdx));
                    ++modelIdx;
                }
            }

            int meshIdx = 0;
            for (int i = 0; i < MESH_TYPE_COUNT; ++i)
            {
                for (auto& [name, mesh] : GetMeshes(MeshType(i)))
                {
                    mMeshCB[gCurrFrame]->CopyElements(mesh->GetMeshConstants(), meshIdx);
                    mesh->SetMeshCBAddress(mMeshCB[gCurrFrame]->GetElementAddress(meshIdx));

                    IndirectCommand indirectCmd;
                    
                    indirectCmd.MeshCBAddr = mesh->GetMeshCBAddress();
                    indirectCmd.SkinnedCBAddr = mesh->GetSkinnedCBAddress();

                    indirectCmd.DispatchMeshArgs.ThreadGroupCountX = ceilf(mesh->GetMeshletSize() * 1.f / WARP_SIZE);
                    indirectCmd.DispatchMeshArgs.ThreadGroupCountY = 1;
                    indirectCmd.DispatchMeshArgs.ThreadGroupCountZ = 1;

                    mIndirectCommandBuffer[gCurrFrame]->CopyElements(&indirectCmd, meshIdx);

                    ++meshIdx;
                }
            }

            mMeshCB[gCurrFrame]->CopyElements(GetSkyBox()->GetMeshConstants(), meshIdx);
            GetSkyBox()->SetMeshCBAddress(mMeshCB[gCurrFrame]->GetElementAddress(meshIdx));
        }

        void Contain(Camera *camera, vector<vector<Mesh *>> &meshes)
        {

        }

        void ClearCullMark()
        {
            static const uint32_t cullMarkClearValue = 0;
            gCommandList->ClearUnorderedAccessViewUint(mInstanceFrustumCulledMarkBuffer->GetGpuUav(), mInstanceFrustumCulledMarkBuffer->GetCpuUav(), mInstanceFrustumCulledMarkBuffer->Get(), &cullMarkClearValue, 0, nullptr);
            gCommandList->ClearUnorderedAccessViewUint(mInstanceOcclusionPassedMarkBuffer->GetGpuUav(), mInstanceOcclusionPassedMarkBuffer->GetCpuUav(), mInstanceOcclusionPassedMarkBuffer->Get(), &cullMarkClearValue, 0, nullptr);

            for (int i = 0; i < MESH_TYPE_COUNT; ++i)
            {
                for (auto& [name, mesh] : GetMeshes((MeshType)i))
                {
                    mesh->ClearCullMark();
                }
            }
        }

        uint32_t GetMeshCBStartOffet(MeshType type)
        {
            return mMeshStartOffset[type];
        }

        uint32_t GetMeshCBIdx()
        {
            return mMeshCB[gCurrFrame]->GetGpuSrvIdx();
        }

        uint32_t GetCommandBufferIdx()
        {
            return mIndirectCommandBuffer[gCurrFrame]->GetGpuSrvIdx();
        }

        uint32_t GetInstanceFrustumCulledMarkBufferIdx()
        {
            return mInstanceFrustumCulledMarkBuffer->GetGpuUavIdx();
        }

        uint32_t GetInstanceOcclusionPassedMarkBufferIdx()
        {
            return mInstanceOcclusionPassedMarkBuffer->GetGpuUavIdx();
        }

        void ExecuteIndirect(StructuredBuffer *indirectCmdBuffer)
        {
            gCommandList->ExecuteIndirect(
                gCommandSignature.Get(),
                indirectCmdBuffer->GetNumElements(),
                indirectCmdBuffer->Get(),
                0,
                indirectCmdBuffer->Get(),
                indirectCmdBuffer->GetCounterOffset());
        }

        void DrawSkyBox(ID3D12PipelineState *skyBoxPSO)
        {
            gCommandList->SetPipelineState(skyBoxPSO);
            gCommandList->SetGraphicsRootConstantBufferView(MESH_CB, GetSkyBox()->GetMeshCBAddress());
            static_cast<ID3D12GraphicsCommandList6*>(gCommandList.Get())->DispatchMesh(1, 1, 1);
        }

    protected:
        void ProcessNode(SceneNode *node, XMMATRIX parentToRoot)
        {
            XMMATRIX toParent = XMLoadFloat4x4(&node->Transformation);
            XMMATRIX world = toParent * parentToRoot;

            for(auto& mesh : node->Meshes)
            {
                mesh->Update(world);
            }

            for (auto& child : node->Children)
            {
                ProcessNode(child.get(), world);
            }
        }

        void InitBuffers()
        {
            for (int i = 0; i < gNumFrame; ++i)
            {
                ResizeBuffer(mIndirectCommandBuffer[i], 1024, sizeof(IndirectCommand), false);
                ResizeBuffer(mMeshCB[i], 1024, sizeof(MeshConstants), true);
                ResizeBuffer(mSkinnedCB[i], 1024, sizeof(SkinnedConstants), true);
            }
        }

        void TestBufferSize(unique_ptr<StructuredBuffer> &buffer, uint32_t numElements)
        {
            if (buffer->GetNumElements() < numElements)
            {
                ResizeBuffer(buffer, numElements, buffer->GetElementSize(), buffer->IsConstant());
            }
        }

        void ResizeBuffer(unique_ptr<StructuredBuffer> &buffer, uint32_t numElements, uint32_t elementSize, bool isConstant)
        {
            buffer = make_unique<StructuredBuffer>(
                numElements,
                elementSize,
                gHeapManager->GetUploadBuffersHeap(),
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_FLAG_NONE,
                isConstant);
        }

        unique_ptr<Model> mSkyBox;
        vector<Light> mLights;
        unique_ptr<SceneNode> mRootNode;

        unordered_map<wstring, unique_ptr<Model>> mModels;
        vector<unordered_map<wstring, Mesh *>> mMeshes;

        vector<unique_ptr<StructuredBuffer>> mIndirectCommandBuffer;
        vector<unique_ptr<StructuredBuffer>> mMeshCB;
        vector<unique_ptr<StructuredBuffer>> mSkinnedCB;

        unique_ptr<RawBuffer> mInstanceFrustumCulledMarkBuffer;
        unique_ptr<RawBuffer> mInstanceOcclusionPassedMarkBuffer;

        uint32_t mMeshStartOffset[MESH_TYPE_COUNT];
    };

    export unique_ptr<Scene> gScene;
}