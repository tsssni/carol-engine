export module carol.renderer.scene.model;
import carol.renderer.scene.mesh;
import carol.renderer.scene.material;
import carol.renderer.scene.texture;
import carol.renderer.scene.skinned_animation;
import carol.renderer.scene.timer;
import carol.renderer.dx12;
import carol.renderer.utils;
import <DirectXMath.h>;
import <DirectXCollision.h>;
import <DirectXPackedVector.h>;
import <vector>;
import <string>;
import <string_view>;
import <memory>;
import <unordered_map>;
import <span>;
import <cmath>;
import <algorithm>;
import <ranges>;

namespace Carol
{
    using std::make_pair;
    using std::make_unique;
    using std::pair;
    using std::span;
    using std::unique_ptr;
    using std::unordered_map;
    using std::vector;
    using std::wstring;
    using std::wstring_view;
    using namespace DirectX;

    export class SkinnedConstants
    {
    public:
        DirectX::XMFLOAT4X4 FinalTransforms[256] = {};
        DirectX::XMFLOAT4X4 HistFinalTransforms[256] = {};
    };

    export class Model
    {
    public:
        Model(TextureManager* textureManager)
            : mSkinnedConstants(make_unique<SkinnedConstants>()), mTextureManager(textureManager)
        {
        }

        virtual ~Model()
        {
            for (auto& path : mTexturePath)
            {
                mTextureManager->UnloadTexture(path);
            }
        }

        bool IsSkinned()const
        {
            return mSkinned;
        }

        void LoadGround(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* cmdList,
            Heap* defaultBuffersHeap,
            Heap* uploadBuffersHeap,
            DescriptorManager* descriptorManager
        )
        {
            XMFLOAT3 pos[4] =
            {
                {-500.0f,0.0f,-500.0f},
                {-500.0f,0.0f,500.0f},
                {500.0f,0.0f,500.0f},
                {500.0f,0.0f,-500.0f}
            };

            XMFLOAT2 texC[4] =
            {
                {0.0f,1000.0f},
                {0.0f,0.0f},
                {1000.0f,0.0f},
                {1000.0f,1000.0f}
            };

            XMFLOAT3 normal = {0.0f, 1.0f, 0.0f};
            XMFLOAT3 tangent = {1.0f, 0.0f, 0.0f};

            vector<Vertex> vertices(4);
            vector<pair<wstring, vector<vector<Vertex>>>> skinnedVertices;

            for (int i = 0; i < 4; ++i)
            {
                vertices[i] = {pos[i], normal, tangent, texC[i]};
            }

            vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};

            mMeshes[L"Ground"] = make_unique<Mesh>(
                vertices,
                skinnedVertices,
                indices,
                false,
                false,
                device,
                cmdList,
                defaultBuffersHeap,
                uploadBuffersHeap,
                descriptorManager);

            mMeshes[L"Ground"]->SetDiffuseMapIdx(mTextureManager->LoadTexture(L"texture\\default_diffuse_map.png", false, device, cmdList, defaultBuffersHeap, uploadBuffersHeap, descriptorManager));
            mMeshes[L"Ground"]->SetNormalMapIdx(mTextureManager->LoadTexture(L"texture\\default_normal_map.png", false, device, cmdList, defaultBuffersHeap, uploadBuffersHeap, descriptorManager));

            mTexturePath.push_back(L"texture\\default_diffuse_map.png");
            mTexturePath.push_back(L"texture\\default_normal_map.png");
        }

        void LoadSkyBox(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* cmdList,
            Heap* defaultBuffersHeap,
            Heap* uploadBuffersHeap,
            DescriptorManager* descriptorManager)
        {
            XMFLOAT3 pos[8] =
                {
                    {-200.0f, -200.0f, 200.0f},
                    {-200.0f, 200.0f, 200.0f},
                    {200.0f, 200.0f, 200.0f},
                    {200.0f, -200.0f, 200.0f},
                    {-200.0f, -200.0f, -200.0f},
                    {-200.0f, 200.0f, -200.0f},
                    {200.0f, 200.0f, -200.0f},
                    {200.0f, -200.0f, -200.0f}};

            vector<Vertex> vertices(8);
            vector<pair<wstring, vector<vector<Vertex>>>> skinnedVertices;

            for (int i = 0; i < 8; ++i)
            {
                vertices[i] = {pos[i]};
            }

            vector<uint32_t> indices = {
                0, 1, 2,
                0, 2, 3,
                4, 5, 1,
                4, 1, 0,
                7, 6, 5,
                7, 5, 4,
                3, 2, 6,
                3, 6, 7,
                1, 5, 6,
                1, 6, 2,
                4, 0, 3,
                4, 3, 7};

            mMeshes[L"SkyBox"] = make_unique<Mesh>(
                vertices,
                skinnedVertices,
                indices,
                false,
                false,
                device,
                cmdList,
                defaultBuffersHeap,
                uploadBuffersHeap,
                descriptorManager);

            mMeshes[L"SkyBox"]->SetDiffuseMapIdx(mTextureManager->LoadTexture(L"texture\\snowcube1024.dds", false, device, cmdList, defaultBuffersHeap, uploadBuffersHeap, descriptorManager));
            mTexturePath.push_back(L"texture\\snowcube1024.dds");
        }

        void ReleaseIntermediateBuffers()
        {
            for (auto &[name, mesh] : mMeshes)
            {
                mesh->ReleaseIntermediateBuffer();
            }
        }

        const Mesh *GetMesh(std::wstring_view meshName)const
        {
            return mMeshes.at(meshName.data()).get();
        }

        const unordered_map<wstring, unique_ptr<Mesh>> &GetMeshes()const
        {
            return mMeshes;
        }

        vector<wstring_view> GetAnimationClips()const
        {
            vector<wstring_view> animations;

            for (auto &[name, animation] : mAnimationClips)
            {
                animations.push_back(wstring_view(name.c_str(), name.size()));
            }

            return animations;
        }

        void SetAnimationClip(wstring_view clipName)
        {
            if (!mSkinned || mAnimationClips.count(clipName.data()) == 0)
            {
                return;
            }

            mClipName = clipName;
            mTimePos = 0.0f;

            for (auto &[name, mesh] : mMeshes)
            {
                mesh->SetAnimationClip(mClipName);
            }
        }

        const SkinnedConstants *GetSkinnedConstants()const
        {
            return mSkinnedConstants.get();
        }

        void SetMeshCBAddress(wstring_view meshName, D3D12_GPU_VIRTUAL_ADDRESS addr)
        {
            mMeshes[meshName.data()]->SetMeshCBAddress(addr);
        }

        void SetSkinnedCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr)
        {
            for (auto& [name, mesh] : mMeshes)
            {
                mesh->SetSkinnedCBAddress(addr);
            }
        }

        void Update(Timer *timer)
        {
            if (mSkinned)
            {
                mTimePos += timer->DeltaTime();

                if (mTimePos > mAnimationClips[mClipName]->GetClipEndTime())
                {
                    mTimePos = 0;
                };

                auto &clip = mAnimationClips[mClipName];
                uint32_t boneCount = mBoneHierarchy.size();

                vector<XMFLOAT4X4> toParentTransforms(boneCount);
                vector<XMFLOAT4X4> toRootTransforms(boneCount);
                vector<XMFLOAT4X4> finalTransforms(boneCount);
                clip->Interpolate(mTimePos, toParentTransforms);

                for (int i = 0; i < boneCount; ++i)
                {
                    XMMATRIX toParent = XMLoadFloat4x4(&toParentTransforms[i]);
                    XMMATRIX parentToRoot = mBoneHierarchy[i] != -1 ? XMLoadFloat4x4(&toRootTransforms[mBoneHierarchy[i]]) : XMMatrixIdentity();

                    XMMATRIX toRoot = toParent * parentToRoot;
                    XMStoreFloat4x4(&toRootTransforms[i], toRoot);
                }

                for (int i = 0; i < boneCount; ++i)
                {
                    XMMATRIX offset = XMLoadFloat4x4(&mBoneOffsets[i]);
                    XMMATRIX toRoot = XMLoadFloat4x4(&toRootTransforms[i]);

                    XMStoreFloat4x4(&finalTransforms[i], XMMatrixTranspose(XMMatrixMultiply(offset, toRoot)));
                }

                std::copy(std::begin(mSkinnedConstants->FinalTransforms), std::end(mSkinnedConstants->FinalTransforms), mSkinnedConstants->HistFinalTransforms);
                std::copy(std::begin(finalTransforms), std::end(finalTransforms), mSkinnedConstants->FinalTransforms);
            }
        }

        void GetSkinnedVertices(wstring_view clipName, const vector<Vertex> &vertices, vector<vector<Vertex>> &skinnedVertices)const
        {
            auto& finalTransforms = mFinalTransforms.at(clipName.data());
            skinnedVertices.resize(finalTransforms.size());
            std::ranges::for_each(skinnedVertices, [&](vector<Vertex>& v) {v.resize(vertices.size()); });

            for (int i = 0; i < finalTransforms.size(); ++i)
            {
                for (int j = 0; j < vertices.size(); ++j)
                {
                    auto& vertex = vertices[j];
                    auto& skinnedVertex = skinnedVertices[i][j];

                    if (vertex.Weights.x == 0.f)
                    {
                        skinnedVertices[i][j] = vertex;
                    }
                    else
                    {
                        float weights[] =
                        {
                            vertex.Weights.x,
                            vertex.Weights.y,
                            vertex.Weights.z,
                            1 - vertex.Weights.x - vertex.Weights.y - vertex.Weights.z
                        };

                        XMMATRIX transform[] =
                        {
                            XMLoadFloat4x4(&finalTransforms[i][vertex.BoneIndices.x]),
                            XMLoadFloat4x4(&finalTransforms[i][vertex.BoneIndices.y]),
                            XMLoadFloat4x4(&finalTransforms[i][vertex.BoneIndices.z]),
                            XMLoadFloat4x4(&finalTransforms[i][vertex.BoneIndices.w])

                        };

                        XMVECTOR pos = { vertex.Pos.x,vertex.Pos.y,vertex.Pos.z,1.f };
                        XMVECTOR normal = { vertex.Normal.x,vertex.Normal.y,vertex.Normal.z,0.f };
                        XMVECTOR skinnedPos = { 0.f,0.f,0.f,0.f };
                        XMVECTOR skinnedNormal = { 0.f,0.f,0.f,0.f };

                        for (int k = 0; k < 4; ++k)
                        {
                            if (weights[k] == 0.f)
                            {
                                break;
                            }

                            skinnedPos += weights[k] * XMVector4Transform(pos, transform[k]);
                            skinnedNormal += weights[k] * XMVector4Transform(normal, transform[k]);
                        }

                        XMStoreFloat3(&skinnedVertex.Pos, skinnedPos);
                        XMStoreFloat3(&skinnedVertex.Normal, skinnedNormal);
                    }
                }
            }
        }

    protected:
        wstring mModelName;
        wstring mTexDir;
        unordered_map<wstring, unique_ptr<Mesh>> mMeshes;

        bool mSkinned = false;
        float mTimePos = 0.f;
        wstring mClipName;

        vector<int> mBoneHierarchy;
        vector<XMFLOAT4X4> mBoneOffsets;

        unordered_map<wstring, unique_ptr<AnimationClip>> mAnimationClips;
        unordered_map<wstring, vector<vector<XMFLOAT4X4>>> mFinalTransforms;
        unique_ptr<SkinnedConstants> mSkinnedConstants;

        TextureManager *mTextureManager;
        vector<wstring> mTexturePath;
    };
}