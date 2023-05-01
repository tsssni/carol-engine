#pragma once
#include <render_pass/render_pass.h>
#include <scene/light.h>
#include <scene/mesh.h>
#include <DirectXMath.h>
#include <memory>
#include <vector>

#define MAX_LIGHTS 16

namespace Carol
{
	class ColorBuffer;
	class StructuredBuffer;
	class StructuredBufferPool;
	class Heap;
	class Scene;
	class DescriptorManager;
	class CullPass;
	class MeshPSO;

	class OitppllNode
	{
	public:
		DirectX::XMFLOAT4 Color;
		uint32_t Depth;
		uint32_t Next;
	};

	class FramePass :public RenderPass
	{
	public:
		FramePass(
			DXGI_FORMAT frameFormat = DXGI_FORMAT_R16G16B16A16_FLOAT,
			DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
			DXGI_FORMAT hiZFormat = DXGI_FORMAT_R32_FLOAT);

		virtual void Draw();
		void Cull();
		void Update(DirectX::XMMATRIX viewProj, DirectX::XMMATRIX histViewProj, DirectX::XMVECTOR eyePos);

		void SetFrameMap(ColorBuffer* frameMap);
		void SetDepthStencilMap(ColorBuffer* depthStencilMap);
		const StructuredBuffer* GetIndirectCommandBuffer(MeshType type)const;

		uint32_t GetPpllUavIdx()const;
		uint32_t GetOffsetUavIdx()const;
		uint32_t GetCounterUavIdx()const;
		uint32_t GetPpllSrvIdx()const;
		uint32_t GetOffsetSrvIdx()const;

	protected:
		virtual void InitPSOs()override;
		virtual void InitBuffers();
		
		void DrawOpaque();
		void DrawTransparent();
		void DrawSkyBox(D3D12_GPU_VIRTUAL_ADDRESS skyBoxMeshCBAddr);

		ColorBuffer* mFrameMap;
		ColorBuffer* mDepthStencilMap;

		std::unique_ptr<StructuredBuffer> mOitppllBuffer;
		std::unique_ptr<RawBuffer> mStartOffsetBuffer; 
		std::unique_ptr<RawBuffer> mCounterBuffer;

		DXGI_FORMAT mFrameFormat;
		DXGI_FORMAT mDepthStencilFormat;

		std::unique_ptr<CullPass> mCullPass;

		std::unique_ptr<MeshPSO> mBlinnPhongStaticMeshPSO;
		std::unique_ptr<MeshPSO> mBlinnPhongSkinnedMeshPSO;
		std::unique_ptr<MeshPSO> mPBRStaticMeshPSO;
		std::unique_ptr<MeshPSO> mPBRSkinnedMeshPSO;
		std::unique_ptr<MeshPSO> mBlinnPhongStaticOitppllMeshPSO;
		std::unique_ptr<MeshPSO> mBlinnPhongSkinnedOitppllMeshPSO;
		std::unique_ptr<MeshPSO> mPBRStaticOitppllMeshPSO;
		std::unique_ptr<MeshPSO> mPBRSkinnedOitppllMeshPSO;
		std::unique_ptr<MeshPSO> mDrawOitppllMeshPSO;
		std::unique_ptr<MeshPSO> mSkyBoxMeshPSO;
	};
}