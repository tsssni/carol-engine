#pragma once
#include <d3d12.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <span>
#include <string>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXCollision.h>

namespace Carol
{
	class StructuredBuffer;
	class RawBuffer;
	class Material;
	class Heap;
	class DescriptorManager;

	enum MeshType
	{
		OPAQUE_STATIC,
		OPAQUE_SKINNED,
		TRANSPARENT_STATIC,
		TRANSPARENT_SKINNED,
		MESH_TYPE_COUNT
	};

	enum OpaqueMeshType
	{
		OPAQUE_MESH_START = 0,
		OPAQUE_MESH_TYPE_COUNT = 2
	};

	enum TransparentMeshType
	{
		TRANSPARENT_MESH_START = 2,
		TRANSPARENT_MESH_TYPE_COUNT = 2
	};


	class MeshConstants
	{
	public:
		DirectX::XMFLOAT4X4 World;
		DirectX::XMFLOAT4X4 HistWorld;

		DirectX::XMFLOAT3 Center;
		float MeshPad0;
		DirectX::XMFLOAT3 Extents;
		float MeshPad1;

		uint32_t MeshletCount = 0;
		uint32_t VertexBufferIdx = 0;
		uint32_t MeshletBufferIdx = 0;
		uint32_t CullDataBufferIdx = 0;

		uint32_t MeshletFrustumCulledMarkBufferIdx = 0;
		uint32_t MeshletNormalConeCulledMarkBufferIdx = 0;
		uint32_t MeshletOcclusionCulledMarkBufferIdx = 0;
		uint32_t MeshletCulledMarkBufferIdx = 0;

		uint32_t DiffuseTextureIdx = 0;
		uint32_t NormalTextureIdx = 0;
		uint32_t EmissiveTextureIdx = 0;
		uint32_t MetallicRoughnessTextureIdx = 0;
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
		uint32_t Vertices[64] = {};
		uint32_t Prims[126] = {};
		uint32_t VertexCount = 0;
		uint32_t PrimCount = 0;
	};

	class CullData
	{
	public:
		DirectX::XMFLOAT3 Center;
		DirectX::XMFLOAT3 Extent;
		DirectX::PackedVector::XMCOLOR NormalCone;
		float ApexOffset = 0.f;
	};

	class Mesh
	{
	public:
		Mesh(
			std::span<Vertex> vertices,
			std::span<std::pair<std::string, std::vector<std::vector<Vertex>>>> skinnedVertices,
			std::span<uint32_t> indices,
			bool isSkinned,
			bool isTransparent);

		uint32_t GetMeshletSize()const;

		void SetDiffuseTextureIdx(uint32_t idx);
		void SetNormalTextureIdx(uint32_t idx);
		void SetEmissiveTextureIdx(uint32_t idx);
		void SetMetallicRoughnessTextureIdx(uint32_t idx);

		void Update(DirectX::XMMATRIX& world);
		void ClearCullMark();
		void SetAnimationClip(std::string_view clipName);

		const MeshConstants* GetMeshConstants()const;
		void SetMeshCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr);
		void SetSkinnedCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr);
		D3D12_GPU_VIRTUAL_ADDRESS GetMeshCBAddress()const;
		D3D12_GPU_VIRTUAL_ADDRESS GetSkinnedCBAddress()const;

		bool IsSkinned()const;
		bool IsTransparent()const;

	protected:
		void LoadVertices();
		void LoadMeshlets();
		void LoadCullData();
		void InitCullMark();
		void ReleaseIntermediateBuffer();

		void LoadMeshletBoundingBox(std::string_view clipName, std::span<std::vector<Vertex>> vertices);
		void LoadMeshletNormalCone(std::string_view clipName, std::span<std::vector<Vertex>> vertices);
	
		DirectX::XMVECTOR LoadConeCenter(const Meshlet& meshlet, std::span<std::vector<Vertex>> vertices);
		float LoadConeSpread(const Meshlet& meshlet, const DirectX::XMVECTOR& normalCone, std::span<std::vector<Vertex>> vertices);
		float LoadConeBottomDist(const Meshlet& meshlet, const DirectX::XMVECTOR& normalCone, std::span<std::vector<Vertex>> vertices);
		float LoadBottomRadius(const Meshlet& meshlet, const DirectX::XMVECTOR& center, const DirectX::XMVECTOR& normalCone, float tanConeSpread, std::span<std::vector<Vertex>> vertices);

		std::span<Vertex> mVertices;
		std::span<std::pair<std::string, std::vector<std::vector<Vertex>>>> mSkinnedVertices;
		std::span<uint32_t> mIndices;

		std::vector<Meshlet> mMeshlets;
		std::unordered_map<std::string, std::vector<CullData>> mCullData;

		std::unique_ptr<StructuredBuffer> mVertexBuffer;
		std::unique_ptr<StructuredBuffer> mMeshletBuffer;
		std::unordered_map<std::string, std::unique_ptr<StructuredBuffer>> mCullDataBuffer;

		std::unique_ptr<RawBuffer> mMeshletFrustumCulledMarkBuffer;
		std::unique_ptr<RawBuffer> mMeshletNormalConeCulledMarkBuffer;
		std::unique_ptr<RawBuffer> mMeshletOcclusionCulledMarkBuffer;
		std::unique_ptr<RawBuffer> mMeshletCulledMarkBuffer;
		
		std::unordered_map<std::string, DirectX::BoundingBox> mBoundingBoxes;

		std::unique_ptr<MeshConstants> mMeshConstants;
		D3D12_GPU_VIRTUAL_ADDRESS mMeshCBAddr = 0;
		D3D12_GPU_VIRTUAL_ADDRESS mSkinnedCBAddr = 0;
		
		bool mSkinned = false;
		bool mTransparent = false;
	};
}
