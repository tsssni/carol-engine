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

	class TextureManager;
	class Model;
	class Timer;
	class Camera;
	class SceneNode;

	class Scene
	{
	public:
		Scene(std::wstring name);
		Scene(const Scene&) = delete;
		Scene(Scene&&) = delete;
		Scene& operator=(const Scene&) = delete;

		void DelayedDelete();
		std::vector<std::wstring_view> GetAnimationClips(std::wstring modelName);
		std::vector<std::wstring_view> GetModelNames();
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

		const std::unordered_map<std::wstring, std::unique_ptr<Model>>& GetModels();
		uint32_t GetModelsCount();

		void SetWorld(std::wstring modelName, DirectX::XMMATRIX world);
		void SetAnimationClip(std::wstring modelName, std::wstring clipName);
		void Update(Camera* camera, Timer* timer);
		void Contain(Camera* camera, std::vector<std::vector<Mesh*>>& meshes);

		void ClearCullMark();
		uint32_t GetMeshCBStartOffet(MeshType type);

		uint32_t GetMeshCBIdx();
		uint32_t GetCommandBufferIdx();
		uint32_t GetInstanceFrustumCulledMarkBufferIdx();
		uint32_t GetInstanceOcclusionPassedMarkBufferIdx();

		void ExecuteIndirect(StructuredBuffer* indirectCmdBuffer);
		void DrawSkyBox(ID3D12PipelineState* skyBoxPSO);

	protected:
		void ProcessNode(SceneNode* node, DirectX::XMMATRIX parentToRoot);
		
		void InitBuffers();
		void TestBufferSize(std::unique_ptr<StructuredBuffer>& buffer, uint32_t numElements);
		void ResizeBuffer(std::unique_ptr<StructuredBuffer>& buffer, uint32_t numElements, uint32_t elementSize, bool isConstant);

		std::unique_ptr<Model> mSkyBox;
		std::vector<Light> mLights;
		std::unique_ptr<SceneNode> mRootNode;

		std::unique_ptr<TextureManager> mTexManager;
		std::unordered_map<std::wstring, std::unique_ptr<Model>> mModels;
		std::vector<std::unordered_map<std::wstring, Mesh*>> mMeshes;

		std::vector<std::unique_ptr<StructuredBuffer>> mIndirectCommandBuffer;
		std::vector<std::unique_ptr<StructuredBuffer>> mMeshCB;
		std::vector<std::unique_ptr<StructuredBuffer>> mSkinnedCB;

		std::unique_ptr<RawBuffer> mInstanceFrustumCulledMarkBuffer;
		std::unique_ptr<RawBuffer> mInstanceOcclusionPassedMarkBuffer;

		uint32_t mMeshStartOffset[MESH_TYPE_COUNT];
	};
}
