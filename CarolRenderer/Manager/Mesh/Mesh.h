#pragma once
#include "../Manager.h"
#include "../../DirectX/Resource.h"
#include <d3d12.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <assimp/aabb.h>
#include <memory>

using std::unique_ptr;


namespace Carol
{
	class SkinnedConstants;
	class Texture;
	class HeapAllocInfo;
	class CircularHeap;

	class MeshConstants
	{
	public:
		DirectX::XMFLOAT4X4 World;
		DirectX::XMFLOAT4X4 HistWorld;

		DirectX::XMFLOAT3 FresnelR0 = { 0.6f,0.6f,0.6f };
		float Roughness = 0.1f;
	};

	class MeshManager : public Manager
	{
	public:
		MeshManager(
			RenderData* renderData,
			bool isSkinned,
			HeapAllocInfo* skinnedCBAllocInfo,
			bool isTransparent,
			uint32_t baseVertexLocation,
			uint32_t startIndexLocation,
			uint32_t indexCount,
			D3D12_VERTEX_BUFFER_VIEW& vertexBufferView,
			D3D12_INDEX_BUFFER_VIEW& indexBufferView);

		virtual void Draw()override;
		virtual void Update()override;
		virtual void OnResize()override;
		virtual void ReleaseIntermediateBuffers()override;

		static void InitMeshCBHeap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);

		D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView();
		D3D12_INDEX_BUFFER_VIEW GetIndexBufferView();
		MeshConstants& GetMeshConstants();

		void LoadDiffuseMap(wstring path);
		void LoadNormalMap(wstring path);

		void SetWorld(DirectX::XMMATRIX world);
		void SetTextureDrawing(bool drawing);

		void SetBoundingBox(aiAABB* boundingBox);
		void SetBoundingBox(DirectX::XMVECTOR boxMin, DirectX::XMVECTOR boxMax);
		void TransformBoundingBox(DirectX::XMMATRIX transform);
		DirectX::BoundingBox GetBoundingBox();

		bool IsSkinned();
		bool IsTransparent();
		
	protected:
		virtual void InitRootSignature()override;
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
		virtual void InitResources()override;
		virtual void InitDescriptors()override;

		HeapAllocInfo* mSkinnedCBAllocInfo;

		uint32_t mBaseVertexLocation;
		uint32_t mStartIndexLocation;
		uint32_t mIndexCount;

		D3D12_VERTEX_BUFFER_VIEW* mVertexBufferView;
		D3D12_INDEX_BUFFER_VIEW* mIndexBufferView;

		unique_ptr<Texture> mDiffuseMap;
		unique_ptr<Texture> mNormalMap;

		DirectX::BoundingBox mBoundingBox;
		DirectX::XMFLOAT3 mBoxMin;
		DirectX::XMFLOAT3 mBoxMax;

		bool mSkinned;
		bool mTransparent;
		bool mTextureDrawing;

		enum
		{
			DIFFUSE_TEXTURE, NORMAL_TEXTURE, TEXTURE_TYPE_COUNT
		};

		unique_ptr<MeshConstants> mMeshConstants;
		unique_ptr<HeapAllocInfo> mMeshCBAllocInfo;
		static unique_ptr<CircularHeap> MeshCBHeap;
	};
}
