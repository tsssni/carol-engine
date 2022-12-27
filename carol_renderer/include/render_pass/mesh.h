#pragma once
#include <render_pass/render_pass.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <assimp/aabb.h>
#include <memory>
#include <unordered_map>

namespace Carol
{
	class GlobalResources;
	class SkinnedConstants;
	class Mesh;
	class Model;
	class Camera;
	class AnimationClip;
	class HeapAllocInfo;
	class CircularHeap;

	class MeshConstants
	{
	public:
		DirectX::XMFLOAT4X4 World;
		DirectX::XMFLOAT4X4 HistWorld;

		DirectX::XMFLOAT3 FresnelR0 = { 0.6f,0.6f,0.6f };
		float Roughness = 0.1f;

		uint32_t DiffuseMapIdx = 0;
		uint32_t NormalMapIdx = 0;
		DirectX::XMUINT2 MeshPad0;
	};

	class SkinnedConstants
	{
	public:
		DirectX::XMFLOAT4X4 FinalTransforms[256];
		DirectX::XMFLOAT4X4 HistoryFinalTransforms[256];
	};

	class MeshData
	{
	public:
		MeshData(
			GlobalResources* globalResources,
			std::unique_ptr<Mesh>& mesh,
			HeapAllocInfo* skinnedAllocInfo,
			D3D12_VERTEX_BUFFER_VIEW* vbv,
			D3D12_INDEX_BUFFER_VIEW* ibv);
		~MeshData();

		void ReleaseIntermediateBuffers();
		void UpdateConstants();

		bool IsTransparent();
		void SetWorld(DirectX::XMMATRIX world);
		DirectX::BoundingBox GetBoundingBox();

		D3D12_VERTEX_BUFFER_VIEW* GetVertexBufferView();
		D3D12_INDEX_BUFFER_VIEW* GetIndexBufferView();
		uint32_t GetBaseVertexLocation();
		uint32_t GetStartIndexLocation();
		uint32_t GetIndexCount();

		HeapAllocInfo* GetMeshCBAllocInfo();
		HeapAllocInfo* GetSkinnedCBAllocInfo();
	protected:
		GlobalResources* mGlobalResources;
		std::unique_ptr<Mesh> mMesh;

		D3D12_VERTEX_BUFFER_VIEW* mVbv;
		D3D12_INDEX_BUFFER_VIEW* mIbv;

		std::unique_ptr<MeshConstants> mMeshConstants;
		std::unique_ptr<HeapAllocInfo> mMeshCBAllocInfo;
		HeapAllocInfo* mSkinnedCBAllocInfo;
	};

	class ModelData
	{
	public:
		ModelData(GlobalResources* globalResources, const std::wstring& path, const std::wstring& texDir, bool isSkinned);
		ModelData(GlobalResources* globalResources);
		void LoadGround();
		void LoadSkyBox();

		bool IsSkinned();
		void SetAnimationClip(std::wstring clipName);
		std::vector<std::wstring> GetAnimationClips();
		void UpdateAnimationClip();
		
		std::vector<MeshData*> GetMeshes();
		void ReleaseIntermediateBuffer();

		void SetWorld(DirectX::XMMATRIX world);
	protected:
		GlobalResources* mGlobalResources;
		std::unique_ptr<Model> mModel;

		float mTimePos = 0.0f;
		std::unique_ptr<SkinnedConstants> mSkinnedConstants;
		std::unique_ptr<HeapAllocInfo> mSkinnedCBAllocInfo;

		DirectX::XMFLOAT4X4 mWorld;
		AnimationClip* mAnimationClip;
		std::unordered_map<std::wstring, std::unique_ptr<MeshData>> mMeshes;
	};

	class MeshesPass : public RenderPass
	{
		friend class MeshData;
		friend class ModelData;

	public:
		MeshesPass(GlobalResources* globalResources);

		virtual void Draw()override;
		virtual void Update()override;
		virtual void OnResize()override;
		virtual void ReleaseIntermediateBuffers()override;

		void ReleaseIntermediateBuffers(const std::wstring& modelName);
	
		void DrawMeshes(const std::vector<ID3D12PipelineState*>& pso);
		void DrawSkyBox(ID3D12PipelineState* skyBoxPSO);

		static void InitMeshCBHeap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
		static void InitSkinnedCBHeap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
		
		void LoadModel(const std::wstring& modelName, const std::wstring& path, const std::wstring texDir, bool isSkinned);
		void SetWorld(const std::wstring& modelName, DirectX::XMMATRIX world);
		void LoadGround();
		void LoadSkyBox();
		void UnloadModel(const std::wstring& modelName);

		std::vector<std::wstring> GetModels();
		void SetAnimationClip(const std::wstring& modelName, const std::wstring& clipName);
		std::vector<std::wstring> GetAnimationClips(std::wstring modelName);

		uint32_t NumTransparentMeshes();
	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitResources()override;
		virtual void InitDescriptors()override;
		
		void Distribute(ModelData* model);
		void Draw(MeshData* mesh);

		enum
		{
			OPAQUE_STATIC, OPAQUE_SKINNED, TRANSPARENT_STATIC, TRANSPARENT_SKINNED, MESH_TYPE_COUNT
		};

		std::unordered_map<std::wstring, std::unique_ptr<ModelData>> mModels;
		std::unique_ptr<ModelData> mSkyBox;
		std::vector<std::vector<MeshData*>> mMeshes;

		static std::unique_ptr<CircularHeap> MeshCBHeap;
		static std::unique_ptr<CircularHeap> SkinnedCBHeap;
	};
}
