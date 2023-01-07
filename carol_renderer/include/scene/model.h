#pragma once
#include <render_pass/global_resources.h>
#include <utils/d3dx12.h>
#include <d3d12.h>
#include <DirectXCollision.h>
#include <DirectXPackedVector.h>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace Carol
{
	class AnimationClip;
	class SkinnedData;
	class Heap;
	class StructuredBuffer;
	class RawBuffer;
	class DescriptorAllocator;
	class Timer;
	class TextureManager;

	class Material
	{
	public:
		DirectX::XMFLOAT3 Emissive = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 Ambient = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 Diffuse = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 Reflective = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 FresnelR0 = {0.2f, 0.2f, 0.2f};
		float Roughness = 0.5f;
	};
	
	class MeshConstants
	{
	public:
		DirectX::XMFLOAT4X4 World;
		DirectX::XMFLOAT4X4 HistWorld;

		DirectX::XMFLOAT3 Center;
		uint32_t MeshletCount;
		DirectX::XMFLOAT3 Extents;
		float MeshPad0;

		DirectX::XMFLOAT3 FresnelR0 = { 0.5f,0.5f,0.5f };
		float Roughness = 0.5f;
	};

	class SkinnedConstants
	{
	public:
		DirectX::XMFLOAT4X4 FinalTransforms[256];
		DirectX::XMFLOAT4X4 HistFinalTransforms[256];
	};

	class Vertex
	{
	public:
		DirectX::XMFLOAT3 Pos = {0.0f ,0.0f ,0.0f};
		DirectX::XMFLOAT3 Normal = {0.0f ,0.0f ,0.0f};
		DirectX::XMFLOAT3 Tangent = {0.0f ,0.0f ,0.0f};
		DirectX::XMFLOAT2 TexC = {0.0f ,0.0f};
		DirectX::XMFLOAT3 Weights = { 0.0f,0.0f,0.0f };
		DirectX::XMUINT4 BoneIndices = {0,0,0,0};
	};

	class Meshlet
	{
	public:
		uint32_t Vertices[64];
		uint32_t Prims[126];
		uint32_t VertexCount = 0;
		uint32_t PrimCount = 0;
	};

	class CullData
	{
	public:
		DirectX::XMFLOAT3 Center;
		DirectX::XMFLOAT3 Extent;
		DirectX::PackedVector::XMCOLOR NormalCone;
		float ApexOffset;
	};

	class Model;
	class OctreeNode;

	enum MeshType
	{
		OPAQUE_STATIC, OPAQUE_SKINNED, TRANSPARENT_STATIC, TRANSPARENT_SKINNED, MESH_TYPE_COUNT
	};

	enum OpaqueMeshType
	{
		OPAQUE_MESH_START = 0, OPAQUE_MESH_TYPE_COUNT = 2
	};

	enum TransparentMeshType
	{
		TRANSPARENT_MESH_START = 2, TRANSPARENT_MESH_TYPE_COUNT = 2
	};

	class Mesh
	{
	public:
		Mesh(
			Model* model,
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmdList,
			Heap* defaultBuffersHeap,
			Heap* uploadBuffersHeap,
			DescriptorAllocator* allocator,
			std::vector<Vertex>& vertices,
			std::vector<uint32_t>& indices,
			bool isSkinned,
			bool isTransparent);

		void ReleaseIntermediateBuffer();

		Material GetMaterial();
		uint32_t GetMeshIdx(uint32_t idx);
		const uint32_t* GetMeshIdxData();
		uint32_t GetMeshletSize();

		void SetMaterial(const Material& mat);
		void SetTexIdx(uint32_t type, uint32_t idx);

		DirectX::BoundingBox GetBoundingBox();
		void SetOctreeNode(OctreeNode* node);
		OctreeNode* GetOctreeNode();

		void Update(DirectX::XMMATRIX& world, CircularHeap* meshCBHeap);
		void ClearCullMark(ID3D12GraphicsCommandList* cmdList);
		D3D12_GPU_VIRTUAL_ADDRESS GetMeshCBGPUVirtualAddress();
		D3D12_GPU_VIRTUAL_ADDRESS GetSkinnedCBGPUVirtualAddress();

		bool IsSkinned();
		bool IsTransparent();

		enum
		{
			VERTEX_IDX, MESHLET_IDX, CULL_DATA_IDX, FRUSTUM_CULLED_MARK_IDX, OCCLUSION_PASSED_MARK, DIFFUSE_IDX, NORMAL_IDX, MESH_IDX_COUNT
		};

	protected:
		void LoadVertices();
		void LoadMeshlets();
		void LoadCullData();
		void InitCullMark();

		void LoadMeshletBoundingBox();
		void LoadMeshletNormalCone();

		DirectX::XMVECTOR LoadConeCenter(const Meshlet& meshlet);
		float LoadConeSpread(const Meshlet& meshlet, const DirectX::XMVECTOR& normalCone);
		float LoadConeBottomDist(const Meshlet& meshlet, const DirectX::XMVECTOR& normalCone);
		float LoadBottomRadius(const Meshlet& meshlet, const DirectX::XMVECTOR& center, const DirectX::XMVECTOR& normalCone, const float& tanConeSpread);

		void BoundingBoxCompare(const DirectX::XMVECTOR& vPos, DirectX::XMFLOAT3& boxMin, DirectX::XMFLOAT3& boxMax);
		void RadiusCompare(const DirectX::XMVECTOR& pos, const DirectX::XMVECTOR& center, const DirectX::XMVECTOR& normalCone, float tanConeSpread, float& radius);

		ID3D12Device* mDevice;
		ID3D12GraphicsCommandList* mCommandList;
		Heap* mDefaultBuffersHeap;
		Heap* mUploadBuffersHeap;
		DescriptorAllocator* mAllocator;

		std::vector<Vertex> mVertices;
		std::vector<uint32_t> mIndices;
		std::vector<Meshlet> mMeshlets;
		std::vector<CullData> mCullData;

		std::unique_ptr<StructuredBuffer> mVertexBuffer;
		std::unique_ptr<StructuredBuffer> mMeshletBuffer;
		std::unique_ptr<StructuredBuffer> mCullDataBuffer;
		std::unique_ptr<RawBuffer> mCullMarkBuffer;
		
		Model* mModel;
		Material mMaterial;
		std::vector<uint32_t> mMeshIdx;

		OctreeNode* mOctreeNode;
		DirectX::BoundingBox mOriginalBoundingBox;
		DirectX::BoundingBox mBoundingBox;

		std::unique_ptr<MeshConstants> mMeshConstants;
		std::unique_ptr<HeapAllocInfo> mMeshCBAllocInfo;
		D3D12_GPU_VIRTUAL_ADDRESS mMeshCBGPUVirtualAddress;
		
		bool mSkinned;
		bool mTransparent;
	};

	class Model
	{
	public:
		Model(ID3D12Device* device,
			ID3D12GraphicsCommandList* cmdList,
			Heap* heap,
			Heap* uploadHeap,
			DescriptorAllocator* allocator);
		~Model();
		
		bool IsSkinned();
		void LoadGround(TextureManager* texManager);
		void LoadSkyBox(TextureManager* texManager);
		
		void ReleaseIntermediateBuffers();

		Mesh* GetMesh(std::wstring meshName);
		const std::unordered_map<std::wstring, std::unique_ptr<Mesh>>& GetMeshes();

		std::vector<int>& GetBoneHierarchy();
		std::vector<DirectX::XMFLOAT4X4>& GetBoneOffsets();
		
		const std::vector<std::vector<std::vector<DirectX::XMFLOAT4X4>>>& GetAnimationTransforms();
		std::vector<std::wstring> GetAnimationClips();
		void SetAnimationClip(std::wstring clipName);
		void Update(Timer* timer, CircularHeap* skinnedCBHeap);
		D3D12_GPU_VIRTUAL_ADDRESS GetSkinnedCBGPUVirtualAddress();

	protected:
		ID3D12Device* mDevice;
		ID3D12GraphicsCommandList* mCommandList;
		Heap* mHeap;
		Heap* mUploadHeap;
		DescriptorAllocator* mAllocator;
		
		std::wstring mTexDir;

		std::wstring mModelName;
		std::unordered_map<std::wstring, std::unique_ptr<Mesh>> mMeshes;

		bool mSkinned;
		float mTimePos;
		std::wstring mClipName;

		std::vector<int> mBoneHierarchy;
		std::vector<DirectX::XMFLOAT4X4> mBoneOffsets;

		std::unordered_map<std::wstring, std::unique_ptr<AnimationClip>> mAnimationClips;
		std::vector<std::vector<std::vector<DirectX::XMFLOAT4X4>>> mAnimationFrames;

		std::unique_ptr<SkinnedConstants> mSkinnedConstants;
		std::unique_ptr<HeapAllocInfo> mSkinnedCBAllocInfo;
		D3D12_GPU_VIRTUAL_ADDRESS mSkinnedCBGPUVirtualAddress;
	};

	
}
