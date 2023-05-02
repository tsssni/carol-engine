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

	class Scene
	{
	public:
		Scene(
			std::wstring_view name);
		Scene(const Scene&) = delete;
		Scene(Scene&&) = delete;
		Scene& operator=(const Scene&) = delete;

		std::vector<std::wstring_view> GetAnimationClips(std::wstring_view modelName)const;
		std::vector<std::wstring_view> GetModelNames()const;
		bool IsAnyOpaqueMeshes()const;
		bool IsAnyTransparentMeshes()const;

		void LoadModel(
			std::wstring_view name,
			std::wstring_view path,
			std::wstring_view textureDir,
			bool isSkinned);
		void LoadSkyBox();

		void UnloadModel(std::wstring_view modelName);
		void ReleaseIntermediateBuffers();
		void ReleaseIntermediateBuffers(std::wstring_view modelName);

		uint32_t GetMeshesCount(MeshType type)const;
		const Mesh* GetSkyBox()const;
		uint32_t GetModelsCount()const;

		void SetWorld(std::wstring_view modelName, DirectX::XMMATRIX world);
		void SetAnimationClip(std::wstring_view modelName, std::wstring_view clipName);
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
	
		std::unique_ptr<Model> mSkyBox;
		std::vector<Light> mLights;
		std::unique_ptr<SceneNode> mRootNode;

		std::unordered_map<std::wstring, std::unique_ptr<Model>> mModels;
		std::vector<std::unordered_map<std::wstring, Mesh*>> mMeshes;

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
