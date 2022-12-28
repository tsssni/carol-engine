#pragma once
#include <scene/light.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>

namespace Carol {

	class Heap;
	class HeapAllocInfo;
	class CircularHeap;
	class CbvSrvUavDescriptorAllocator;
	class Mesh;
	class TextureManager;
	class Model;
	class Timer;

	class SceneNode
	{
	public:
		SceneNode();
		std::wstring Name;
		std::vector<Mesh*> Meshes;
		std::vector<std::unique_ptr<SceneNode>> Children;

		DirectX::XMFLOAT4X4 Transformation;
		DirectX::XMFLOAT4X4 HistTransformation;
	
		std::unique_ptr<HeapAllocInfo> WorldAllocInfo;
		std::unique_ptr<HeapAllocInfo> HistWorldAllocInfo;
	};

	class RenderNode
	{
	public:
		Mesh* Mesh;
		D3D12_GPU_VIRTUAL_ADDRESS WorldGPUVirtualAddress;
		D3D12_GPU_VIRTUAL_ADDRESS HistWorldGPUVirtualAddress;
	};

	class Scene
	{
	public:
		Scene(std::wstring name, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, Heap* texHeap, Heap* uploadHeap, CbvSrvUavDescriptorAllocator* allocator);
		Scene(const Scene&) = delete;
		Scene(Scene&&) = delete;
		Scene& operator=(const Scene&) = delete;

		void SetCurrFrame(uint32_t currFrame);
		void DelayedDelete();
		std::vector<std::wstring> GetAnimationClips(std::wstring modelName);
		std::vector<std::wstring> GetModelNames();

		void LoadModel(ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap, std::wstring name, std::wstring path, std::wstring textureDir, bool isSkinned);
		void LoadGround(ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap);
		void LoadSkyBox(ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap);

		void UnloadModel(std::wstring modelName);
		void ReleaseIntermediateBuffers();
		void ReleaseIntermediateBuffers(std::wstring modelName);

		std::vector<RenderNode>& GetMeshes(uint32_t type);
		RenderNode GetSkyBox();

		void SetWorld(std::wstring modelName, DirectX::XMMATRIX world);
		void SetAnimationClip(std::wstring modelName, std::wstring clipName);
		void Update(Timer& timer);
		void ProcessNode(SceneNode* node, DirectX::XMMATRIX parentToRoot, DirectX::XMMATRIX histParentToRoot);

		enum
		{
			OPAQUE_STATIC, OPAQUE_SKINNED, TRANSPARENT_STATIC, TRANSPARENT_SKINNED, MESH_TYPE_COUNT
		};

	protected:
		void UpdateSkyBox();

		std::unordered_map<std::wstring, std::unique_ptr<Model>> mModels;
		std::unique_ptr<Model> mSkyBox;
		std::vector<Light> mLights;
		std::unique_ptr<SceneNode> mRootNode;

		std::unique_ptr<TextureManager> mTexManager;
		std::unique_ptr<CircularHeap> mTransformationCBHeap;
		std::unique_ptr<CircularHeap> mSkinnedCBHeap;
		std::vector<std::vector<RenderNode>> mMeshes;

		RenderNode mSkyBoxNode;
	};
}
