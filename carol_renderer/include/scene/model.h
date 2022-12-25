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
	class DefaultResource;

	class Material
	{
	public:
		DirectX::XMFLOAT3 Emissive = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 Ambient = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 Diffuse = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 Specular = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 FresnelR0 = {0.2f, 0.2f, 0.2f};
		float Roughness = 0.5f;
	};

	class Mesh
	{
	public:
		Mesh(
			bool isTransparent,
			uint32_t baseVertexLocation,
			uint32_t startIndexLocation,
			uint32_t indexCount);
		
		uint32_t GetBaseVertexLocation();
		uint32_t GetStartIndexLocation();
		uint32_t GetIndexCount();

		Material GetMaterial();
		std::wstring GetDiffuseMapPath();
		std::wstring GetNormalMapPath();

		void SetMaterial(const Material& mat);
		void SetDiffuseMapPath(const std::wstring& path);
		void SetNormalMapPath(const std::wstring& path);

		void SetBoundingBox(DirectX::XMVECTOR boxMin, DirectX::XMVECTOR boxMax);
		void TransformBoundingBox(DirectX::XMMATRIX transform);
		DirectX::BoundingBox GetBoundingBox();

		bool IsTransparent();
	protected:
		uint32_t mBaseVertexLocation;
		uint32_t mStartIndexLocation;
		uint32_t mIndexCount;
		
		Material mMaterial;
		std::wstring mDiffuseMapPath;
		std::wstring mNormalMapPath;

		DirectX::BoundingBox mBoundingBox;
		DirectX::XMFLOAT3 mBoxMin;
		DirectX::XMFLOAT3 mBoxMax;

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
		Model(bool isSkinned = false);
		~Model();
		void LoadVerticesAndIndices(ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap);
		
		bool IsSkinned();
		void LoadGround(ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap);
		void LoadSkyBox(ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap);
		
		void ReleaseIntermediateBuffers();
		D3D12_VERTEX_BUFFER_VIEW* GetVertexBufferView();
		D3D12_INDEX_BUFFER_VIEW* GetIndexBufferView();
		const std::unordered_map<std::wstring, std::unique_ptr<Mesh>>& GetMeshes();

		std::vector<int>& GetBoneHierarchy();
		std::vector<DirectX::XMFLOAT4X4>& GetBoneOffsets();

		AnimationClip* GetAnimationClip(std::wstring clip);
		std::vector<std::wstring> GetAnimationClips();
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
	};

	
}
