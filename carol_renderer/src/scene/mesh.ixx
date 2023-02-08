export module carol.renderer.scene.mesh;
import carol.renderer.scene.material;
import carol.renderer.dx12;
import carol.renderer.utils;
import <memory>;
import <unordered_map>;
import <vector>;
import <span>;
import <string>;
import <cmath>;
import <DirectXMath.h>;
import <DirectXPackedVector.h>;
import <DirectXCollision.h>;

namespace Carol
{
    using std::make_unique;
    using std::pair;
    using std::span;
    using std::unique_ptr;
    using std::unordered_map;
    using std::vector;
    using std::wstring;
    using std::wstring_view;
    using namespace DirectX;

    export class MeshConstants
    {
    public:
        XMFLOAT4X4 World;
        XMFLOAT4X4 HistWorld;

        XMFLOAT3 Center;
        float MeshPad0;
        XMFLOAT3 Extents;
        float MeshPad1;

        XMFLOAT3 FresnelR0 = {0.5f, 0.5f, 0.5f};
        float Roughness = 0.5f;

        uint32_t MeshletCount = 0;
        uint32_t VertexBufferIdx = 0;
        uint32_t MeshletBufferIdx = 0;
        uint32_t CullDataBufferIdx = 0;

        uint32_t MeshletFrustumCulledMarkBufferIdx = 0;
        uint32_t MeshletOcclusionCulledMarkBufferIdx = 0;

        uint32_t DiffuseMapIdx = 0;
        uint32_t NormalMapIdx = 0;
    };

    export class Vertex
    {
    public:
        XMFLOAT3 Pos = {0.0f, 0.0f, 0.0f};
        XMFLOAT3 Normal = {0.0f, 0.0f, 0.0f};
        XMFLOAT3 Tangent = {0.0f, 0.0f, 0.0f};
        XMFLOAT2 TexC = {0.0f, 0.0f};
        XMFLOAT3 Weights = {0.0f, 0.0f, 0.0f};
        XMUINT4 BoneIndices = {0, 0, 0, 0};
    };

    class Meshlet
    {
    public:
        uint32_t Vertices[64] = {};
        uint32_t Prims[126] = {};
        uint32_t VertexCount = 0;
        uint32_t PrimCount = 0;
    };

    class CullData
    {
    public:
        XMFLOAT3 Center;
        XMFLOAT3 Extent;
        PackedVector::XMCOLOR NormalCone;
        float ApexOffset = 0.f;
    };

    export enum MeshType {
        OPAQUE_STATIC,
        OPAQUE_SKINNED,
        TRANSPARENT_STATIC,
        TRANSPARENT_SKINNED,
        MESH_TYPE_COUNT
    };

    export enum OpaqueMeshType {
        OPAQUE_MESH_START = 0,
        OPAQUE_MESH_TYPE_COUNT = 2
    };

    export enum TransparentMeshType {
        TRANSPARENT_MESH_START = 2,
        TRANSPARENT_MESH_TYPE_COUNT = 2
    };

    void BoundingBoxCompare(const XMFLOAT3 &pos, XMFLOAT3 &boxMin, XMFLOAT3 &boxMax)
    {
        boxMin.x = fmin(boxMin.x, pos.x);
        boxMin.y = fmin(boxMin.y, pos.y);
        boxMin.z = fmin(boxMin.z, pos.z);

        boxMax.x = fmax(boxMax.x, pos.x);
        boxMax.y = fmax(boxMax.y, pos.y);
        boxMax.z = fmax(boxMax.z, pos.z);
    }

    void RadiusCompare(const XMVECTOR &pos, const XMVECTOR &center, const XMVECTOR &normalCone, float tanConeSpread, float &radius)
    {
        float height = XMVector3Dot(pos - center, normalCone).m128_f32[0];
        float posRadius = XMVector3Length(pos - center - normalCone * height).m128_f32[0] - height * tanConeSpread;

        if (posRadius > radius)
        {
            radius = posRadius;
        }
    }

    export class Mesh
    {
    public:
        Mesh(
            span<Vertex> vertices,
            span<pair<wstring, vector<vector<Vertex>>>> skinnedVertices,
            span<uint32_t> indices,
            bool isSkinned,
            bool isTransparent,
            ID3D12Device* device,
			ID3D12GraphicsCommandList* cmdList,
			Heap* defaultBuffersHeap,
			Heap* uploadBuffersHeap,
			DescriptorManager* descriptorManager)
            : mVertices(vertices),
              mSkinnedVertices(skinnedVertices),
              mIndices(indices),
              mMaterial(make_unique<Material>()),
              mMeshConstants(make_unique<MeshConstants>()),
              mSkinned(isSkinned),
              mTransparent(isTransparent)
        {
            LoadVertices(device, cmdList, defaultBuffersHeap, uploadBuffersHeap, descriptorManager);
            LoadMeshlets(device, cmdList, defaultBuffersHeap, uploadBuffersHeap, descriptorManager);
            LoadCullData(device, cmdList, defaultBuffersHeap, uploadBuffersHeap, descriptorManager);
            InitCullMark(device, cmdList, defaultBuffersHeap, uploadBuffersHeap, descriptorManager);
        }

        void ReleaseIntermediateBuffer()
        {
            mVertexBuffer->ReleaseIntermediateBuffer();
            mMeshletBuffer->ReleaseIntermediateBuffer();

            for (auto &[name, buffer] : mCullDataBuffer)
            {
                buffer->ReleaseIntermediateBuffer();
            }

            mMeshlets.clear();
            mMeshlets.shrink_to_fit();

            for (auto &[name, data] : mCullData)
            {
                data.clear();
                data.shrink_to_fit();
            }
        }

        const Material *GetMaterial()const
        {
            return mMaterial.get();
        }

        uint32_t GetMeshletSize()const
        {
            return mMeshConstants->MeshletCount;
        }

        void SetMaterial(const Material &mat)
        {
            *mMaterial = mat;
        }

        void SetDiffuseMapIdx(uint32_t idx)
        {
            mMeshConstants->DiffuseMapIdx = idx;
        }

        void SetNormalMapIdx(uint32_t idx)
        {
            mMeshConstants->NormalMapIdx = idx;
        }

        void Update(XMMATRIX &world)
        {
            mMeshConstants->FresnelR0 = mMaterial->FresnelR0;
            mMeshConstants->Roughness = mMaterial->Roughness;
            mMeshConstants->HistWorld = mMeshConstants->World;
            XMStoreFloat4x4(&mMeshConstants->World, XMMatrixTranspose(world));
        }

        void ClearCullMark(ID3D12GraphicsCommandList* cmdList)
        {
            static const uint32_t cullMarkClearValue = 0;
            cmdList->ClearUnorderedAccessViewUint(mMeshletFrustumCulledMarkBuffer->GetGpuUav(), mMeshletFrustumCulledMarkBuffer->GetCpuUav(), mMeshletFrustumCulledMarkBuffer->Get(), &cullMarkClearValue, 0, nullptr);
            cmdList->ClearUnorderedAccessViewUint(mMeshletOcclusionPassedMarkBuffer->GetGpuUav(), mMeshletOcclusionPassedMarkBuffer->GetCpuUav(), mMeshletOcclusionPassedMarkBuffer->Get(), &cullMarkClearValue, 0, nullptr);
        }

        void SetAnimationClip(wstring_view clipName)
        {
            wstring name(clipName);
            mMeshConstants->CullDataBufferIdx = mCullDataBuffer[name]->GetGpuSrvIdx();
            mMeshConstants->Center = mBoundingBoxes[name].Center;
            mMeshConstants->Extents = mBoundingBoxes[name].Extents;
        }

        const MeshConstants *GetMeshConstants()const
        {
            return mMeshConstants.get();
        }

        void SetMeshCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr)
        {
            mMeshCBAddr = addr;
        }

        void SetSkinnedCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr)
        {
            mSkinnedCBAddr = addr;
        }

        D3D12_GPU_VIRTUAL_ADDRESS GetMeshCBAddress()const
        {
            return mMeshCBAddr;
        }

        D3D12_GPU_VIRTUAL_ADDRESS GetSkinnedCBAddress()const
        {
            return mSkinnedCBAddr;
        }

        bool IsSkinned()const
        {
            return mSkinned;
        }

        bool IsTransparent()const
        {
            return mTransparent;
        }

    protected:
        void LoadVertices(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* cmdList,
            Heap* defaultBuffersHeap,
            Heap* uploadBuffersHeap,
            DescriptorManager* descriptorManager)
        {
            mVertexBuffer = make_unique<StructuredBuffer>(
                mVertices.size(),
                sizeof(Vertex),
                device,
                defaultBuffersHeap,
                descriptorManager,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

            mVertexBuffer->CopySubresources(cmdList, uploadBuffersHeap, mVertices.data(), mVertices.size() * sizeof(Vertex));
            mMeshConstants->VertexBufferIdx = mVertexBuffer->GetGpuSrvIdx();
        }

        void LoadMeshlets(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* cmdList,
            Heap* defaultBuffersHeap,
            Heap* uploadBuffersHeap,
            DescriptorManager* descriptorManager)
        {
            Meshlet meshlet = {};
            vector<uint8_t> vertices(mVertices.size(), 0xff);

            for (int i = 0; i < mIndices.size(); i += 3)
            {
                uint32_t index[3] = {mIndices[i], mIndices[i + 1], mIndices[i + 2]};
                uint32_t meshletIndex[3] = {vertices[index[0]], vertices[index[1]], vertices[index[2]]};

                if (meshlet.VertexCount + (meshletIndex[0] == 0xff) + (meshletIndex[1] == 0xff) + (meshletIndex[2] == 0xff) > 64 || meshlet.PrimCount + 1 > 126)
                {
                    mMeshlets.push_back(meshlet);

                    meshlet = {};

                    for (auto &index : meshletIndex)
                    {
                        index = 0xff;
                    }

                    for (auto &vertex : vertices)
                    {
                        vertex = 0xff;
                    }
                }

                for (int j = 0; j < 3; ++j)
                {
                    if (meshletIndex[j] == 0xff)
                    {
                        meshletIndex[j] = meshlet.VertexCount;
                        meshlet.Vertices[meshlet.VertexCount] = index[j];
                        vertices[index[j]] = meshlet.VertexCount++;
                    }
                }

                // Pack the indices
                meshlet.Prims[meshlet.PrimCount++] =
                    (meshletIndex[0] & 0x3ff) |
                    ((meshletIndex[1] & 0x3ff) << 10) |
                    ((meshletIndex[2] & 0x3ff) << 20);
            }

            if (meshlet.PrimCount)
            {
                mMeshlets.push_back(meshlet);
            }

            mMeshletBuffer = make_unique<StructuredBuffer>(
                mMeshlets.size(),
                sizeof(Meshlet),
                device,
                defaultBuffersHeap,
                descriptorManager,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

            mMeshletBuffer->CopySubresources(cmdList, uploadBuffersHeap, mMeshlets.data(), mMeshlets.size() * sizeof(Meshlet));
            mMeshConstants->MeshletBufferIdx = mMeshletBuffer->GetGpuSrvIdx();
            mMeshConstants->MeshletCount = mMeshlets.size();
        }

        void LoadCullData(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* cmdList,
            Heap* defaultBuffersHeap,
            Heap* uploadBuffersHeap,
            DescriptorManager* descriptorManager)
        {
            static wstring staticName = L"mesh";
            vector<vector<Vertex>> vertices;
            pair<wstring, vector<vector<Vertex>>> staticPair;

            if (!mSkinned)
            {
                vertices.emplace_back(mVertices.begin(), mVertices.end());
                staticPair = make_pair(staticName, std::move(vertices));
                mSkinnedVertices = span(&staticPair, 1);
            }

            for (auto &[name, vertices] : mSkinnedVertices)
            {
                mCullData[name].resize(mMeshlets.size());
                LoadMeshletBoundingBox(name, vertices);
                LoadMeshletNormalCone(name, vertices);

                mCullDataBuffer[name] = make_unique<StructuredBuffer>(
                    mCullData[name].size(),
                    sizeof(CullData),
                    device,
                    defaultBuffersHeap,
                    descriptorManager,
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                mCullDataBuffer[name]->CopySubresources(cmdList, uploadBuffersHeap, mCullData[name].data(), mCullData[name].size() * sizeof(CullData));
            }

            if (!mSkinned)
            {
                mMeshConstants->CullDataBufferIdx = mCullDataBuffer[staticName]->GetGpuSrvIdx();
                mMeshConstants->Center = mBoundingBoxes[staticName].Center;
                mMeshConstants->Extents = mBoundingBoxes[staticName].Extents;
            }
        }

        void InitCullMark(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* cmdList,
            Heap* defaultBuffersHeap,
            Heap* uploadBuffersHeap,
            DescriptorManager* descriptorManager)
        {
            uint32_t byteSize = ceilf(mMeshlets.size() / 8.f);

            mMeshletFrustumCulledMarkBuffer = make_unique<RawBuffer>(
                byteSize,
                device,
                defaultBuffersHeap,
                descriptorManager,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

            mMeshConstants->MeshletFrustumCulledMarkBufferIdx = mMeshletFrustumCulledMarkBuffer->GetGpuUavIdx();

            mMeshletOcclusionPassedMarkBuffer = make_unique<RawBuffer>(
                byteSize,
                device,
                defaultBuffersHeap,
                descriptorManager,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

            mMeshConstants->MeshletOcclusionCulledMarkBufferIdx = mMeshletOcclusionPassedMarkBuffer->GetGpuUavIdx();
        }

        void LoadMeshletBoundingBox(wstring_view clipName, span<vector<Vertex>> vertices)
        {
            wstring name(clipName);
            XMFLOAT3 meshBoxMin = {D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX};
            XMFLOAT3 meshBoxMax = {-D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX};

            for (int i = 0; i < mMeshlets.size(); ++i)
            {
                auto &meshlet = mMeshlets[i];
                XMFLOAT3 meshletBoxMin = {D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX};
                XMFLOAT3 meshletBoxMax = {-D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX};

                for (auto &criticalFrameVertices : vertices)
                {
                    for (int j = 0; j < meshlet.VertexCount; ++j)
                    {
                        auto &pos = criticalFrameVertices[meshlet.Vertices[j]].Pos;
                        BoundingBoxCompare(pos, meshBoxMin, meshBoxMax);
                        BoundingBoxCompare(pos, meshletBoxMin, meshletBoxMax);
                    }
                }
                int a = sizeof(CullData);

                BoundingBox box;
                BoundingBox::CreateFromPoints(box, XMLoadFloat3(&meshletBoxMin), XMLoadFloat3(&meshletBoxMax));

                mCullData[name][i].Center = box.Center;
                mCullData[name][i].Extent = box.Extents;
            }

            BoundingBox::CreateFromPoints(mBoundingBoxes[name], XMLoadFloat3(&meshBoxMin), XMLoadFloat3(&meshBoxMax));
        }

        void LoadMeshletNormalCone(wstring_view clipName, span<vector<Vertex>> vertices)
        {
            wstring name(clipName);

            for (int i = 0; i < mMeshlets.size(); ++i)
            {
                auto &meshlet = mMeshlets[i];
                XMVECTOR normalCone = LoadConeCenter(meshlet, vertices);
                float cosConeSpread = LoadConeSpread(meshlet, normalCone, vertices);

                if (cosConeSpread <= 0.f)
                {
                    mCullData[name][i].NormalCone = {0.f, 0.f, 0.f, 1.f};
                }
                else
                {
                    float sinConeSpread = std::sqrt(1.f - cosConeSpread * cosConeSpread);
                    float tanConeSpread = sinConeSpread / cosConeSpread;

                    mCullData[name][i].NormalCone = {
                        (normalCone.m128_f32[2] + 1.f) * 0.5f,
                        (normalCone.m128_f32[1] + 1.f) * 0.5f,
                        (normalCone.m128_f32[0] + 1.f) * 0.5f,
                        sinConeSpread};

                    float bottomDist = LoadConeBottomDist(meshlet, normalCone, vertices);
                    XMVECTOR center = XMLoadFloat3(&mCullData[name][i].Center);

                    float centerToBottomDist = XMVector3Dot(center, normalCone).m128_f32[0] - bottomDist;
                    XMVECTOR bottomCenter = center - centerToBottomDist * normalCone;
                    float radius = LoadBottomRadius(meshlet, bottomCenter, normalCone, tanConeSpread, vertices);

                    mCullData[name][i].ApexOffset = centerToBottomDist + radius / tanConeSpread;
                }
            }
        }

        XMVECTOR LoadConeCenter(const Meshlet &meshlet, span<vector<Vertex>> vertices)
        {
            XMFLOAT3 normalBoxMin = {D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX};
            XMFLOAT3 normalBoxMax = {-D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX};

            if (mSkinned)
            {
                for (auto &criticalFrameVertices : vertices)
                {
                    for (int i = 0; i < meshlet.VertexCount; ++i)
                    {
                        auto &normal = criticalFrameVertices[meshlet.Vertices[i]].Normal;
                        BoundingBoxCompare(normal, normalBoxMin, normalBoxMax);
                    }
                }
            }

            BoundingBox box;
            BoundingBox::CreateFromPoints(box, XMLoadFloat3(&normalBoxMin), XMLoadFloat3(&normalBoxMax));
            return XMLoadFloat3(&box.Center);
        }

        float LoadConeSpread(const Meshlet &meshlet, const XMVECTOR &normalCone, span<vector<Vertex>> vertices)
        {
            float cosConeSpread = 1.f;

            for (auto &criticalFrameVertices : vertices)
            {
                for (int i = 0; i < meshlet.VertexCount; ++i)
                {
                    auto normal = XMLoadFloat3(&criticalFrameVertices[meshlet.Vertices[i]].Normal);
                    cosConeSpread = fmin(cosConeSpread, XMVector3Dot(normalCone, XMVector3Normalize(normal)).m128_f32[0]);
                }
            }

            return cosConeSpread;
        }

        float LoadConeBottomDist(const Meshlet &meshlet, const XMVECTOR &normalCone, span<vector<Vertex>> vertices)
        {
            float bd = D3D12_FLOAT32_MAX;

            for (auto &criticalFrameVertices : vertices)
            {
                for (int i = 0; i < meshlet.VertexCount; ++i)
                {
                    auto pos = XMLoadFloat3(&criticalFrameVertices[meshlet.Vertices[i]].Pos);
                    float dot = XMVector3Dot(normalCone, pos).m128_f32[0];

                    if (dot < bd)
                    {
                        bd = dot;
                    }
                }
            }

            return bd;
        }

        float LoadBottomRadius(const Meshlet &meshlet, const XMVECTOR &center, const XMVECTOR &normalCone, float tanConeSpread, span<vector<Vertex>> vertices)
        {
            float radius = 0.f;

            for (auto &criticalFrameVertices : vertices)
            {
                for (int i = 0; i < meshlet.VertexCount; ++i)
                {
                    auto pos = XMLoadFloat3(&criticalFrameVertices[meshlet.Vertices[i]].Pos);
                    RadiusCompare(pos, center, normalCone, tanConeSpread, radius);
                }
            }

            return radius;
        }

        span<Vertex> mVertices;
        span<pair<wstring, vector<vector<Vertex>>>> mSkinnedVertices;
        span<uint32_t> mIndices;

        vector<Meshlet> mMeshlets;
        unordered_map<wstring, vector<CullData>> mCullData;

        unique_ptr<StructuredBuffer> mVertexBuffer;
        unique_ptr<StructuredBuffer> mMeshletBuffer;
        unordered_map<wstring, unique_ptr<StructuredBuffer>> mCullDataBuffer;
        unique_ptr<RawBuffer> mMeshletFrustumCulledMarkBuffer;
        unique_ptr<RawBuffer> mMeshletOcclusionPassedMarkBuffer;

        unique_ptr<Material> mMaterial;
        unordered_map<wstring, BoundingBox> mBoundingBoxes;

        unique_ptr<MeshConstants> mMeshConstants;
        D3D12_GPU_VIRTUAL_ADDRESS mMeshCBAddr = 0;
        D3D12_GPU_VIRTUAL_ADDRESS mSkinnedCBAddr = 0;

        bool mSkinned = false;
        bool mTransparent = false;
    };
}