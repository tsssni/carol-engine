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
			std::wstring_view name,
			ID3D12Device* device,
			Heap* defaultBuffersHeap,
			Heap* uploadBuffersHeap,
			DescriptorManager* descriptorManager);
		Scene(const Scene&) = delete;
		Scene(Scene&&) = delete;
		Scene& operator=(const Scene&) = delete;

		std::vector<std::wstring_view> GetAnimationClips(std::wstring_view modelName)const;
		std::vector<std::wstring_view> GetModelNames()const;
		bool IsAnyTransparentMeshes()const;

		void LoadModel(
			std::wstring_view name,
			std::wstring_view path,
			std::wstring_view textureDir,
			bool isSkinned,
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmdList,
			Heap* defaultBuffersHeap,
			Heap* uploadBuffersHeap,
			DescriptorManager* descriptorManager,
			TextureManager* textureManager);
		void LoadSkyBox(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmdList,
			Heap* defaultBuffersHeap,
			Heap* uploadBuffersHeap,
			DescriptorManager* descriptorManager,
			TextureManager* textureManager);

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

		void ClearCullMark(ID3D12GraphicsCommandList* cmdList);
		uint32_t GetMeshCBStartOffet(MeshType type)const;

		uint32_t GetMeshCBIdx()const;
		uint32_t GetCommandBufferIdx()const;
		uint32_t GetInstanceFrustumCulledMarkBufferIdx()const;
		uint32_t GetInstanceOcclusionPassedMarkBufferIdx()const;

	protected:
		void ProcessNode(SceneNode* node, DirectX::XMMATRIX parentToRoot);

		void InitBuffers(
			ID3D12Device* device,
			Heap* defaultBuffersHeap,
			Heap* uploadBuffersHeap,
			DescriptorManager* descriptorManager);
	
		std::unique_ptr<Model> mSkyBox;
		std::vector<Light> mLights;
		std::unique_ptr<SceneNode> mRootNode;

		std::unordered_map<std::wstring, std::unique_ptr<Model>> mModels;
		std::vector<std::unordered_map<std::wstring, Mesh*>> mMeshes;

		std::unique_ptr<StructuredBuffer> mIndirectCommandBuffer;
		std::unique_ptr<StructuredBuffer> mMeshCB;
		std::unique_ptr<StructuredBuffer> mSkinnedCB;

		std::unique_ptr<StructuredBufferPool> mIndirectCommandBufferPool;
		std::unique_ptr<StructuredBufferPool> mMeshCBPool;
		std::unique_ptr<StructuredBufferPool> mSkinnedCBPool;

		std::unique_ptr<RawBuffer> mInstanceFrustumCulledMarkBuffer;
		std::unique_ptr<RawBuffer> mInstanceOcclusionPassedMarkBuffer;

		uint32_t mMeshStartOffset[MESH_TYPE_COUNT];
	};
}
