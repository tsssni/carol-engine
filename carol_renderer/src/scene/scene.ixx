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
        Scene(
            wstring_view name,
            ID3D12Device* device,
            Heap* defaultBuffersHeap,
            Heap* uploadBuffersHeap,
            DescriptorManager* descriptorManager)
            :mRootNode(make_unique<SceneNode>()),
            mMeshes(MESH_TYPE_COUNT)
        {
            mRootNode->Name = name;
            InitBuffers(device, defaultBuffersHeap, uploadBuffersHeap, descriptorManager);
        }

        Scene(const Scene &) = delete;
        Scene(Scene &&) = delete;
        Scene &operator=(const Scene &) = delete;

        vector<wstring_view> GetAnimationClips(wstring_view modelName)const
        {
            return mModels.at(modelName.data())->GetAnimationClips();
        }

        vector<wstring_view> GetModelNames()const
        {
            vector<wstring_view> models;
            for (auto& [name, model] : mModels)
            {
                models.push_back(wstring_view(name.c_str(), name.size()));
            }

            return models;
        }

        bool IsAnyTransparentMeshes()const
        {
            return mMeshes[TRANSPARENT_STATIC].size() + mMeshes[TRANSPARENT_SKINNED].size();
        }

        void LoadModel(
            wstring_view name,
            wstring_view path,
            wstring_view textureDir,
            bool isSkinned,
            ID3D12Device* device,
            ID3D12GraphicsCommandList* cmdList,
            Heap* defaultBuffersHeap,
            Heap* uploadBuffersHeap,
            DescriptorManager* descriptorManager,
            TextureManager* textureManager)
        {
            mRootNode->Children.push_back(make_unique<SceneNode>());
            auto& node = mRootNode->Children.back();
            node->Children.push_back(make_unique<SceneNode>());

            node->Name = name;
            mModels[node->Name] = make_unique<AssimpModel>(
                node->Children[0].get(),
                path,
                textureDir,
                isSkinned,
                device,
                cmdList,
                defaultBuffersHeap,
                uploadBuffersHeap,
                descriptorManager,
                textureManager);

            for (auto& [name, mesh] : mModels[node->Name]->GetMeshes())
            {
                wstring meshName = node->Name + L'_' + name;
                uint32_t type = uint32_t(mesh->IsSkinned()) | (uint32_t(mesh->IsTransparent()) << 1);
                mMeshes[type][meshName] = mesh.get();
            }
        }

        void LoadGround(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* cmdList,
            Heap* defaultBuffersHeap,
            Heap* uploadBuffersHeap,
            DescriptorManager* descriptorManager,
            TextureManager* textureManager)
        {
            mModels[L"Ground"] = make_unique<Model>(textureManager);
            mModels[L"Ground"]->LoadGround(
                device,
                cmdList,
                defaultBuffersHeap,
                uploadBuffersHeap,
                descriptorManager);
            
            mRootNode->Children.push_back(make_unique<SceneNode>());
            auto& node = mRootNode->Children.back();

            node->Name = L"Ground";
            node->Children.push_back(make_unique<SceneNode>());
            node->Children[0]->Meshes.push_back(const_cast<Mesh*>(mModels[L"Ground"]->GetMesh(L"Ground")));

            for (auto& [name, mesh] : mModels[L"Ground"]->GetMeshes())
            {
                wstring meshName = L"Ground_" + name;
                mMeshes[OPAQUE_STATIC][meshName] = mesh.get();
            }
        }

        void LoadSkyBox(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* cmdList,
            Heap* defaultBuffersHeap,
            Heap* uploadBuffersHeap,
            DescriptorManager* descriptorManager,
            TextureManager* textureManager)
        {
            mSkyBox = make_unique<Model>(textureManager);
            mSkyBox->LoadSkyBox(
                device,
                cmdList,
                defaultBuffersHeap,
                uploadBuffersHeap,
                descriptorManager
            );
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
                uint32_t type = uint32_t(mesh->IsSkinned()) | (uint32_t(mesh->IsTransparent()) << 1);
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

        uint32_t GetMeshesCount(MeshType type)const
        {
            return mMeshes[type].size();
        }

        const Mesh *GetSkyBox()const
        {
            return mSkyBox->GetMesh(L"SkyBox");
        }

        uint32_t GetModelsCount()const
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

        void Update(Timer* timer, uint64_t cpuFenceValue, uint64_t completedFenceValue)
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
            
            mIndirectCommandBufferPool->DiscardBuffer(mIndirectCommandBuffer.release(), cpuFenceValue);
            mMeshCBPool->DiscardBuffer(mMeshCB.release(), cpuFenceValue);
            mSkinnedCBPool->DiscardBuffer(mSkinnedCB.release(), cpuFenceValue);
            
            mIndirectCommandBuffer = mIndirectCommandBufferPool->RequestBuffer(completedFenceValue, totalMeshCount);
            mMeshCB = mMeshCBPool->RequestBuffer(completedFenceValue, totalMeshCount);
            mSkinnedCB = mSkinnedCBPool->RequestBuffer(completedFenceValue, GetModelsCount());

            int modelIdx = 0;
            for (auto& [name, model] : mModels)
            {
                if (model->IsSkinned())
                {
                    mSkinnedCB->CopyElements(model->GetSkinnedConstants(), modelIdx);
                    model->SetSkinnedCBAddress(mSkinnedCB->GetElementAddress(modelIdx));
                    ++modelIdx;
                }
            }

            int meshIdx = 0;
            for (int i = 0; i < MESH_TYPE_COUNT; ++i)
            {
                for (auto& [name, mesh] : mMeshes[i])
                {
                    mMeshCB->CopyElements(mesh->GetMeshConstants(), meshIdx);
                    mesh->SetMeshCBAddress(mMeshCB->GetElementAddress(meshIdx));

                    IndirectCommand indirectCmd;
                    
                    indirectCmd.MeshCBAddr = mesh->GetMeshCBAddress();
                    indirectCmd.SkinnedCBAddr = mesh->GetSkinnedCBAddress();

                    indirectCmd.DispatchMeshArgs.ThreadGroupCountX = ceilf(mesh->GetMeshletSize() * 1.f / WARP_SIZE);
                    indirectCmd.DispatchMeshArgs.ThreadGroupCountY = 1;
                    indirectCmd.DispatchMeshArgs.ThreadGroupCountZ = 1;

                    mIndirectCommandBuffer->CopyElements(&indirectCmd, meshIdx);

                    ++meshIdx;
                }
            }

            mMeshCB->CopyElements(GetSkyBox()->GetMeshConstants(), meshIdx);
            mSkyBox->SetMeshCBAddress(L"SkyBox", mMeshCB->GetElementAddress(meshIdx));
        }

        void Contain(Camera *camera, vector<vector<Mesh *>> &meshes)
        {

        }

        void ClearCullMark(ID3D12GraphicsCommandList* cmdList)
        {
            static const uint32_t cullMarkClearValue = 0;
            cmdList->ClearUnorderedAccessViewUint(mInstanceFrustumCulledMarkBuffer->GetGpuUav(), mInstanceFrustumCulledMarkBuffer->GetCpuUav(), mInstanceFrustumCulledMarkBuffer->Get(), &cullMarkClearValue, 0, nullptr);
            cmdList->ClearUnorderedAccessViewUint(mInstanceOcclusionPassedMarkBuffer->GetGpuUav(), mInstanceOcclusionPassedMarkBuffer->GetCpuUav(), mInstanceOcclusionPassedMarkBuffer->Get(), &cullMarkClearValue, 0, nullptr);

            for (int i = 0; i < MESH_TYPE_COUNT; ++i)
            {
                for (auto& [name, mesh] : mMeshes[i])
                {
                    mesh->ClearCullMark(cmdList);
                }
            }
        }

        uint32_t GetMeshCBStartOffet(MeshType type)const
        {
            return mMeshStartOffset[type];
        }

        uint32_t GetMeshCBIdx()const
        {
            return mMeshCB->GetGpuSrvIdx();
        }

        uint32_t GetCommandBufferIdx()const
        {
            return mIndirectCommandBuffer->GetGpuSrvIdx();
        }

        uint32_t GetInstanceFrustumCulledMarkBufferIdx()const
        {
            return mInstanceFrustumCulledMarkBuffer->GetGpuUavIdx();
        }

        uint32_t GetInstanceOcclusionPassedMarkBufferIdx()const
        {
            return mInstanceOcclusionPassedMarkBuffer->GetGpuUavIdx();
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

        void InitBuffers(
            ID3D12Device* device,
            Heap* defaultBuffersHeap,
            Heap* uploadBuffersHeap,
            DescriptorManager* descriptorManager)
        {
            mInstanceFrustumCulledMarkBuffer = make_unique<RawBuffer>(
                2 << 16,
                device,
                defaultBuffersHeap,
                descriptorManager,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

            mInstanceOcclusionPassedMarkBuffer = make_unique<RawBuffer>(
                2 << 16,
                device,
                defaultBuffersHeap,
                descriptorManager,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

            mIndirectCommandBufferPool = make_unique<StructuredBufferPool>(
                1024,
                sizeof(IndirectCommand),
                device,
                uploadBuffersHeap,
                descriptorManager,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_FLAG_NONE,
                false);

            mMeshCBPool = make_unique<StructuredBufferPool>(
                1024,
                sizeof(IndirectCommand),
                device,
                uploadBuffersHeap,
                descriptorManager,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_FLAG_NONE,
                true);

            mSkinnedCBPool = make_unique<StructuredBufferPool>(
                1024,
                sizeof(IndirectCommand),
                device,
                uploadBuffersHeap,
                descriptorManager,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_FLAG_NONE,
                true);
        }

        unique_ptr<Model> mSkyBox;
        vector<Light> mLights;
        unique_ptr<SceneNode> mRootNode;

        unordered_map<wstring, unique_ptr<Model>> mModels;
        vector<unordered_map<wstring, Mesh *>> mMeshes;

        unique_ptr<StructuredBuffer> mIndirectCommandBuffer;
		unique_ptr<StructuredBuffer> mMeshCB;
		unique_ptr<StructuredBuffer> mSkinnedCB;

		unique_ptr<StructuredBufferPool> mIndirectCommandBufferPool;
		unique_ptr<StructuredBufferPool> mMeshCBPool;
		unique_ptr<StructuredBufferPool> mSkinnedCBPool;

        unique_ptr<RawBuffer> mInstanceFrustumCulledMarkBuffer;
        unique_ptr<RawBuffer> mInstanceOcclusionPassedMarkBuffer;

        uint32_t mMeshStartOffset[MESH_TYPE_COUNT];
    };
}