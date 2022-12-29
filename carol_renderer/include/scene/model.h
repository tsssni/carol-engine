#pragma once
#include <render_pass/global_resources.h>
#include <utils/d3dx12.h>
#include <d3d12.h>
#include <DirectXCollision.h>
#include <string>
#include <memory>
#include <unordered_map>

namespace Carol
{
	class AnimationClip;
	class SkinnedData;
	class Heap;
	class CircularHeap;
	class DefaultResource;
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
		DirectX::XMFLOAT3 FresnelR0 = { 0.5f,0.5f,0.5f };
		float Roughness = 0.5f;
	};

	class SkinnedConstants
	{
	public:
		DirectX::XMFLOAT4X4 FinalTransforms[256];
		DirectX::XMFLOAT4X4 HistFinalTransforms[256];
	};

	class Model;
	class OctreeNode;

	class Mesh
	{
	public:
		Mesh(
			Model* model,
			D3D12_VERTEX_BUFFER_VIEW* vertexBufferView,
			D3D12_INDEX_BUFFER_VIEW* indexBufferView,
			uint32_t baseVertexLocation,
			uint32_t startIndexLocation,
			uint32_t indexCount,
			bool isSkinned,
			bool isTransparent);
		~Mesh();
		
		D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView();
		D3D12_INDEX_BUFFER_VIEW GetIndexBufferView();
		D3D12_GPU_VIRTUAL_ADDRESS GetSkinnedCBGPUVirtualAddress();

		uint32_t GetBaseVertexLocation();
		uint32_t GetStartIndexLocation();
		uint32_t GetIndexCount();

		Material GetMaterial();
		std::vector<uint32_t> GetTexIdx();

		void SetMaterial(const Material& mat);
		void SetTexIdx(uint32_t type, uint32_t idx);
		void SetOctreeNode(OctreeNode* node);
		OctreeNode* GetOctreeNode();

		void SetBoundingBox(DirectX::XMVECTOR boxMin, DirectX::XMVECTOR boxMax);
		bool TransformBoundingBox(DirectX::XMMATRIX transform);
		DirectX::BoundingBox GetBoundingBox();

		bool IsSkinned();
		bool IsTransparent();

		enum
		{
			DIFFUSE_IDX, NORMAL_IDX, TEX_IDX_COUNT
		};
	protected:

		D3D12_VERTEX_BUFFER_VIEW* mVertexBufferView;
		D3D12_INDEX_BUFFER_VIEW* mIndexBufferView;
		uint32_t mBaseVertexLocation;
		uint32_t mStartIndexLocation;
		uint32_t mIndexCount;
		
		Model* mModel;
		Material mMaterial;
		std::vector<uint32_t> mTexIdx;

		DirectX::BoundingBox mBoundingBox;
		DirectX::XMFLOAT3 mBoxMin;
		DirectX::XMFLOAT3 mBoxMax;

		OctreeNode* mOctreeNode;
		
		bool mSkinned;
		bool mTransparent;
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

	class Model
	{
	public:
		Model();
		~Model();
		void LoadVerticesAndIndices(ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap);
		
		bool IsSkinned();
		void LoadGround(ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap, TextureManager* texManager);
		void LoadSkyBox(ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap, TextureManager* texManager);
		
		void ReleaseIntermediateBuffers();

		Mesh* GetMesh(std::wstring meshName);
		const std::unordered_map<std::wstring, std::unique_ptr<Mesh>>& GetMeshes();

		std::vector<int>& GetBoneHierarchy();
		std::vector<DirectX::XMFLOAT4X4>& GetBoneOffsets();

		void ComputeSkinnedBoundingBox();
		std::vector<std::wstring> GetAnimationClips();
		void SetAnimationClip(std::wstring clipName);
		void UpdateAnimationClip(Timer& timer, CircularHeap* skinnedCBHeap);
		D3D12_GPU_VIRTUAL_ADDRESS GetSkinnedCBGPUVirtualAddress();

	protected:
		std::vector<Vertex> mVertices;
		std::vector<uint32_t> mIndices;
		std::unique_ptr<DefaultResource> mVertexBufferGpu;
		std::unique_ptr<DefaultResource> mIndexBufferGpu;

		D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
		D3D12_INDEX_BUFFER_VIEW mIndexBufferView;
		
		std::wstring mTexDir;

		std::wstring mModelName;
		std::unordered_map<std::wstring, std::unique_ptr<Mesh>> mMeshes;

		bool mSkinned;
		float mTimePos;
		std::wstring mClipName;

		std::vector<int> mBoneHierarchy;
		std::vector<DirectX::XMFLOAT4X4> mBoneOffsets;

		std::unordered_map<std::wstring, std::unique_ptr<AnimationClip>> mAnimationClips;

		std::unique_ptr<SkinnedConstants> mSkinnedConstants;
		std::unique_ptr<HeapAllocInfo> mSkinnedCBAllocInfo;
		D3D12_GPU_VIRTUAL_ADDRESS mSkinnedCBGPUVirtualAddress;
	};

	
}
