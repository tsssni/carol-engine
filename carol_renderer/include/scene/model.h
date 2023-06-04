#pragma once
#include <scene/mesh.h>
#include <utils/d3dx12.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <DirectXPackedVector.h>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <unordered_map>
#include <span>

namespace Carol
{
	class AnimationClip;
	class TextureManager;
	class Timer;
	class Mesh;
	class Camera;
	class StructuredBuffer;
	class FrameBufferAllocator;
	
	class SkinnedConstants
	{
	public:
		DirectX::XMFLOAT4X4 FinalTransforms[256] = {};
		DirectX::XMFLOAT4X4 HistFinalTransforms[256] = {};
	};

	class ModelNode
	{
	public:
		ModelNode();
		std::string Name;
		std::vector<Mesh*> Meshes;
		std::vector<std::unique_ptr<ModelNode>> Children;
		DirectX::XMFLOAT4X4 Transformation;
	};


	class Model
	{
	public:
		Model();
		virtual ~Model();
		
		bool IsSkinned()const;
		
		const Mesh* GetMesh(std::string_view meshName)const;
		const std::unordered_map<std::string, std::unique_ptr<Mesh>>& GetMeshes()const;

		std::vector<std::string_view> GetAnimationClips()const;
		void SetAnimationClip(std::string_view clipName);

		const SkinnedConstants* GetSkinnedConstants()const;
		void SetMeshCBAddress(std::string_view meshName, D3D12_GPU_VIRTUAL_ADDRESS addr);
		void SetSkinnedCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr);

		void Update(Timer* timer);
		void GetFinalTransforms(std::string_view clipName, float t, std::vector<DirectX::XMFLOAT4X4>& toRootTransforms);
		void GetSkinnedVertices(std::string_view clipName, std::span<Vertex> vertices, std::vector<std::vector<Vertex>>& skinnedVertices)const;

	protected:
		std::string mModelName;
		std::string mTexDir;
		std::unordered_map<std::string, std::unique_ptr<Mesh>> mMeshes;

		std::vector<Vertex> mVertices;
		std::vector<uint32_t> mIndices;

		bool mSkinned = false;
		float mTimePos = 0.f;
		std::string mClipName;

		std::vector<int> mBoneHierarchy;
		std::vector<DirectX::XMFLOAT4X4> mBoneOffsets;

		std::unordered_map<std::string, std::unique_ptr<AnimationClip>> mAnimationClips;
		std::unordered_map<std::string, std::vector<std::vector<DirectX::XMFLOAT4X4>>> mFrameTransforms;
		std::unique_ptr<SkinnedConstants> mSkinnedConstants;

		std::vector<std::string> mTexturePath;
	};

	class ModelManager
	{
	public:
		ModelManager(
			std::string_view name);
		ModelManager(const ModelManager&) = delete;
		ModelManager(ModelManager&&) = delete;
		ModelManager& operator=(const ModelManager&) = delete;

		std::vector<std::string_view> GetAnimationClips(std::string_view modelName)const;
		std::vector<std::string_view> GetModelNames()const;
		bool IsAnyOpaqueMeshes()const;
		bool IsAnyTransparentMeshes()const;

		void LoadModel(
			std::string_view name,
			std::string_view path,
			std::string_view textureDir,
			bool isSkinned);
		void UnloadModel(std::string_view modelName);

		uint32_t GetMeshesCount(MeshType type)const;
		uint32_t GetModelsCount()const;

		void SetWorld(std::string_view modelName, DirectX::XMMATRIX world);
		void SetAnimationClip(std::string_view modelName, std::string_view clipName);
		void Update(Timer* timer, uint64_t cpuFenceValue, uint64_t completedFenceValue);

		uint32_t GetMeshBufferIdx(MeshType type)const;
		uint32_t GetCommandBufferIdx(MeshType type)const;
		uint32_t GetInstanceFrustumCulledMarkBufferIdx(MeshType type)const;
		uint32_t GetInstanceOcclusionCulledMarkBufferIdx(MeshType type)const;
		uint32_t GetInstanceCulledMarkBufferIdx(MeshType type)const;

	protected:
		void ProcessNode(ModelNode* node, DirectX::XMMATRIX parentToRoot);
		void InitBuffers();
	
		std::unique_ptr<ModelNode> mRootNode;

		std::unordered_map<std::string, std::unique_ptr<Model>> mModels;
		std::vector<std::unordered_map<std::string, Mesh*>> mMeshes;

		std::vector<std::unique_ptr<StructuredBuffer>> mIndirectCommandBuffer;
		std::vector<std::unique_ptr<StructuredBuffer>> mMeshBuffer;
		std::unique_ptr<StructuredBuffer> mSkinnedBuffer;

		std::unique_ptr<FrameBufferAllocator> mIndirectCommandBufferAllocator;
		std::unique_ptr<FrameBufferAllocator> mMeshBufferAllocator;
		std::unique_ptr<FrameBufferAllocator> mSkinnedBufferAllocator;

		std::vector<std::unique_ptr<RawBuffer>> mInstanceFrustumCulledMarkBuffer;
		std::vector<std::unique_ptr<RawBuffer>> mInstanceOcclusionCulledMarkBuffer;
		std::vector<std::unique_ptr<RawBuffer>> mInstanceCulledMarkBuffer;

		uint32_t mMeshStartOffset[MESH_TYPE_COUNT];
	};

}
