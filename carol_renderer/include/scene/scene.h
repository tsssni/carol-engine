#pragma once
#include <scene/light.h>
#include <scene/model.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <string_view>

#define WARP_SIZE 32

namespace Carol {

	class Model;
	class Timer;
	class Camera;
	class SceneNode;
	class Heap;
	class DescriptorManager;
	class StructuredBufferPool;

	class SceneManager
	{
	public:
		SceneManager(
			std::string_view name);
		SceneManager(const SceneManager&) = delete;
		SceneManager(SceneManager&&) = delete;
		SceneManager& operator=(const SceneManager&) = delete;

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
		void ReleaseIntermediateBuffers();
		void ReleaseIntermediateBuffers(std::string_view modelName);

		uint32_t GetMeshesCount(MeshType type)const;
		uint32_t GetModelsCount()const;

		void SetWorld(std::string_view modelName, DirectX::XMMATRIX world);
		void SetAnimationClip(std::string_view modelName, std::string_view clipName);
		void Update(Timer* timer, uint64_t cpuFenceValue, uint64_t completedFenceValue);
		void Contain(Camera* camera, std::vector<std::vector<Mesh*>>& meshes);

		void ClearCullMark();
		uint32_t GetMeshCBStartOffet(MeshType type)const;

		uint32_t GetMeshBufferIdx()const;
		uint32_t GetCommandBufferIdx()const;
		uint32_t GetInstanceFrustumCulledMarkBufferIdx()const;
		uint32_t GetInstanceOcclusionCulledMarkBufferIdx()const;
		uint32_t GetInstanceCulledMarkBufferIdx()const;

	protected:
		void ProcessNode(SceneNode* node, DirectX::XMMATRIX parentToRoot);
		void InitBuffers();
	
		std::vector<Light> mLights;
		std::unique_ptr<SceneNode> mRootNode;

		std::unordered_map<std::string, std::unique_ptr<Model>> mModels;
		std::vector<std::unordered_map<std::string, Mesh*>> mMeshes;

		std::unique_ptr<StructuredBuffer> mIndirectCommandBuffer;
		std::unique_ptr<StructuredBuffer> mMeshBuffer;
		std::unique_ptr<StructuredBuffer> mSkinnedBuffer;

		std::unique_ptr<StructuredBufferPool> mIndirectCommandBufferPool;
		std::unique_ptr<StructuredBufferPool> mMeshBufferPool;
		std::unique_ptr<StructuredBufferPool> mSkinnedBufferPool;

		std::unique_ptr<RawBuffer> mInstanceFrustumCulledMarkBuffer;
		std::unique_ptr<RawBuffer> mInstanceOcclusionCulledMarkBuffer;
		std::unique_ptr<RawBuffer> mInstanceCulledMarkBuffer;

		uint32_t mMeshStartOffset[MESH_TYPE_COUNT];
	};
}
