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

namespace Carol {

	class Heap;
	class HeapAllocInfo;
	class CircularHeap;
	class DescriptorAllocator;
	class Mesh;
	class TextureManager;
	class Model;
	class Timer;
	class Camera;

	class OctreeNode
	{
	public:
		std::vector<Mesh*> Meshes;
		std::vector<std::unique_ptr<OctreeNode>> Children;
		DirectX::BoundingBox BoundingBox;
		DirectX::BoundingBox LooseBoundingBox;
	};

	class Octree
	{
	public:
		Octree(DirectX::BoundingBox sceneBoundingBox, float looseFactor = 1.5f);
		Octree(DirectX::XMVECTOR boxMin, DirectX::XMVECTOR boxMax, float looseFactor = 1.5f);

		void Insert(Mesh* mesh);
		void Delete(Mesh* mesh);
		void Contain(Camera* camera, std::vector<std::vector<Mesh*>>& meshes);
	protected:
		bool ProcessNode(OctreeNode* node, Mesh* mesh);
		void ProcessContainment(OctreeNode* node, Camera* camera, std::vector<std::vector<Mesh*>>& meshes);

		DirectX::BoundingBox ExtendBoundingBox(const DirectX::BoundingBox& box);
		void DevideBoundingBox(OctreeNode* node);
		
		std::unique_ptr<OctreeNode> mRootNode;
		float mLooseFactor;
	};

	class SceneNode
	{
	public:
		SceneNode();
		std::wstring Name;
		std::vector<Mesh*> Meshes;
		std::vector<std::unique_ptr<SceneNode>> Children;
		DirectX::XMFLOAT4X4 Transformation;
	};

	class RenderNode
	{
	public:
		Mesh* Mesh;
		D3D12_GPU_VIRTUAL_ADDRESS WorldGPUVirtualAddress;
	};

	class Scene
	{
	public:
		Scene(std::wstring name, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, Heap* defaultBuffersHeap, Heap* texHeap, Heap* uploadBuffersHeap, DescriptorAllocator* allocator);
		Scene(const Scene&) = delete;
		Scene(Scene&&) = delete;
		Scene& operator=(const Scene&) = delete;

		void DelayedDelete(uint32_t currFrame);
		std::vector<std::wstring> GetAnimationClips(std::wstring modelName);
		std::vector<std::wstring> GetModelNames();
		bool IsAnyTransparentMeshes();

		void LoadModel(std::wstring name, std::wstring path, std::wstring textureDir, bool isSkinned);
		void LoadGround();
		void LoadSkyBox();

		void UnloadModel(std::wstring modelName);
		void ReleaseIntermediateBuffers();
		void ReleaseIntermediateBuffers(std::wstring modelName);

		const std::unordered_map<std::wstring, Mesh*>& GetMeshes(MeshType type);
		uint32_t GetMeshesCount(MeshType type);
		Mesh* GetSkyBox();

		void SetWorld(std::wstring modelName, DirectX::XMMATRIX world);
		void SetAnimationClip(std::wstring modelName, std::wstring clipName);
		void Update(Camera* camera, Timer* timer);
		void Contain(Camera* camera, std::vector<std::vector<Mesh*>>& meshes);

	protected:
		void ProcessNode(SceneNode* node, DirectX::XMMATRIX parentToRoot);

		ID3D12Device* mDevice;
		ID3D12GraphicsCommandList* mCommandList;
		Heap* mDefaultBuffersHeap;
		Heap* mUploadBuffersHeap;
		DescriptorAllocator* mAllocator;

		std::unordered_map<std::wstring, std::unique_ptr<Model>> mModels;
		std::unique_ptr<Model> mSkyBox;
		std::vector<Light> mLights;

		std::unique_ptr<SceneNode> mRootNode;

		std::unique_ptr<TextureManager> mTexManager;
		std::unique_ptr<CircularHeap> mMeshCBHeap;
		std::unique_ptr<CircularHeap> mSkinnedCBHeap;
		std::vector<std::unordered_map<std::wstring, Mesh*>> mMeshes;
	};
}
