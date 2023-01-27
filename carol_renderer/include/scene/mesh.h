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

	class MeshConstants
	{
	public:
		DirectX::XMFLOAT4X4 World;
		DirectX::XMFLOAT4X4 HistWorld;

		DirectX::XMFLOAT3 Center;
		float MeshPad0;
		DirectX::XMFLOAT3 Extents;
		float MeshPad1;

		DirectX::XMFLOAT3 FresnelR0 = { 0.5f,0.5f,0.5f };
		float Roughness = 0.5f;

		uint32_t MeshletCount;
		uint32_t VertexBufferIdx;
		uint32_t MeshletBufferIdx;
		uint32_t CullDataBufferIdx;

		uint32_t MeshletFrustumCulledMarkBufferIdx;
		uint32_t MeshletOcclusionCulledMarkBufferIdx;

		uint32_t DiffuseMapIdx;
		uint32_t NormalMapIdx;
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

	class Mesh
	{
	public:
		Mesh(
			std::span<Vertex> vertices,
			std::span<std::pair<std::wstring, std::vector<std::vector<Vertex>>>> skinnedVertices,
			std::span<uint32_t> indices,
			bool isSkinned,
			bool isTransparent);

		void ReleaseIntermediateBuffer();

		const Material* GetMaterial();
		uint32_t GetMeshletSize();

		void SetMaterial(const Material& mat);
		void SetDiffuseMapIdx(uint32_t idx);
		void SetNormalMapIdx(uint32_t idx);

		void Update(DirectX::XMMATRIX& world);
		void ClearCullMark();
		void SetAnimationClip(std::wstring_view clipName);

		MeshConstants* GetMeshConstants();
		void SetMeshCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr);
		void SetSkinnedCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr);
		D3D12_GPU_VIRTUAL_ADDRESS GetMeshCBAddress();
		D3D12_GPU_VIRTUAL_ADDRESS GetSkinnedCBAddress();

		bool IsSkinned();
		bool IsTransparent();

	protected:
		void LoadVertices();
		void LoadMeshlets();
		void LoadCullData();
		void InitCullMark();

		void LoadMeshletBoundingBox(std::wstring_view clipName, std::span<std::vector<Vertex>> vertices);
		void LoadMeshletNormalCone(std::wstring_view clipName, std::span<std::vector<Vertex>> vertices);
	
		DirectX::XMVECTOR LoadConeCenter(const Meshlet& meshlet, std::span<std::vector<Vertex>> vertices);
		float LoadConeSpread(const Meshlet& meshlet, const DirectX::XMVECTOR& normalCone, std::span<std::vector<Vertex>> vertices);
		float LoadConeBottomDist(const Meshlet& meshlet, const DirectX::XMVECTOR& normalCone, std::span<std::vector<Vertex>> vertices);
		float LoadBottomRadius(const Meshlet& meshlet, const DirectX::XMVECTOR& center, const DirectX::XMVECTOR& normalCone, float tanConeSpread, std::span<std::vector<Vertex>> vertices);

		std::span<Vertex> mVertices;
		std::span<std::pair<std::wstring, std::vector<std::vector<Vertex>>>> mSkinnedVertices;
		std::span<uint32_t> mIndices;

		std::vector<Meshlet> mMeshlets;
		std::unordered_map<std::wstring, std::vector<CullData>> mCullData;

		std::unique_ptr<StructuredBuffer> mVertexBuffer;
		std::unique_ptr<StructuredBuffer> mMeshletBuffer;
		std::unordered_map<std::wstring, std::unique_ptr<StructuredBuffer>> mCullDataBuffer;
		std::unique_ptr<RawBuffer> mMeshletFrustumCulledMarkBuffer;
		std::unique_ptr<RawBuffer> mMeshletOcclusionPassedMarkBuffer;
		
		std::unique_ptr<Material> mMaterial;
		std::unordered_map<std::wstring, DirectX::BoundingBox> mBoundingBoxes;

		std::unique_ptr<MeshConstants> mMeshConstants;
		D3D12_GPU_VIRTUAL_ADDRESS mMeshCBAddr;
		D3D12_GPU_VIRTUAL_ADDRESS mSkinnedCBAddr;
		
		bool mSkinned;
		bool mTransparent;
	};
}
