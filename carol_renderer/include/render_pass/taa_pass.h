#pragma once
#include <render_pass/render_pass.h>
#include <scene/mesh.h>
#include <utils/d3dx12.h>
#include <DirectXMath.h>
#include <memory>

namespace Carol
{
	class ColorBuffer;
	class Resource;
	
	class TaaPass : public RenderPass
	{
	public:
		TaaPass(
			ID3D12Device* device,
			DXGI_FORMAT frameFormat,
			DXGI_FORMAT velocityMapFormat = DXGI_FORMAT_R16G16_FLOAT,
			DXGI_FORMAT frameDsvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
		TaaPass(const TaaPass&) = delete;
		TaaPass(TaaPass&&) = delete;
		TaaPass& operator=(const TaaPass&) = delete;

		virtual void Draw(ID3D12GraphicsCommandList* cmdList)override;
		
		void SetCurrBackBuffer(Resource* currBackBuffer);
		void SetIndirectCommandBuffer(MeshType type, const StructuredBuffer* indirectCommandBuffer);
		void SetCurrBackBufferRtv(D3D12_CPU_DESCRIPTOR_HANDLE currBackBufferRtv);

		void GetHalton(float& proj0,float& proj1)const;
		void SetHistViewProj(DirectX::XMMATRIX& histViewProj);
		DirectX::XMMATRIX GetHistViewProj()const;
		
		uint32_t GetVeloctiySrvIdx()const;
		uint32_t GetHistFrameSrvIdx()const;

	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs(ID3D12Device* device)override;
		virtual void InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)override;

		void InitHalton();
		float RadicalInversion(int base, int num);

        void DrawVelocityMap(ID3D12GraphicsCommandList* cmdList);
        void DrawOutput(ID3D12GraphicsCommandList* cmdList);

		std::unique_ptr<ColorBuffer> mHistFrameMap;
		std::unique_ptr<ColorBuffer> mVelocityMap;
		std::unique_ptr<ColorBuffer> mVelocityDepthStencilMap;

		DXGI_FORMAT mVelocityMapFormat;
		DXGI_FORMAT mVelocityDsvFormat;
		DXGI_FORMAT mFrameFormat;

		D3D12_CPU_DESCRIPTOR_HANDLE mCurrBackBufferRtv;

		DirectX::XMFLOAT2 mHalton[8];
		DirectX::XMMATRIX mHistViewProj;
		
		std::vector<const StructuredBuffer*> mIndirectCommandBuffer;
		Resource* mCurrBackBuffer;

	};
}


