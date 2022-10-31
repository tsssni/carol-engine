#pragma once
#include "../DirectX/Buffer.h"
#include "../DirectX/d3dx12.h"
#include <DirectXMath.h>
#include <memory>

using std::unique_ptr;

namespace Carol
{
	class GraphicsCommandList;
	class GraphicsPipelineState;
	class Device;
	class RootSignature;
	class FrameResource;
	class Resource;

	class TaaManager
	{
	public:
		void InitTaa(ID3D12Device* device, uint32_t width, uint32_t height);
		void OnResize(uint32_t width, uint32_t height);
		void RebuildDescriptors(Resource* depthStencilBuffer);

		void InitDescriptors(
			Resource* depthStencilBuffer,
			CD3DX12_CPU_DESCRIPTOR_HANDLE cpuSrv,
			CD3DX12_GPU_DESCRIPTOR_HANDLE gpuSrv,
			CD3DX12_CPU_DESCRIPTOR_HANDLE cpuRtv,
			uint32_t cbvSrvUavDescriptorSize,
			uint32_t rtvDescriptorSize
		);

		DefaultBuffer* GetHistFrameMap();
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetHistFrameMapSrv();

		DefaultBuffer* GetCurrFrameMap();
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetCurrFrameMapSrv();
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCurrFrameMapRtv();

		DefaultBuffer* GetVelocityMap();
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetVelocityMapSrv();
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetVelocityMapRtv();

		CD3DX12_GPU_DESCRIPTOR_HANDLE GetDepthSrv();

		uint32_t GetNumSrvs();
		uint32_t GetNumRtvs();

		void SetFrameFormat(DXGI_FORMAT format);
		DXGI_FORMAT GetFrameFormat();
		void SetVelocityFormat(DXGI_FORMAT format);
		DXGI_FORMAT GetVelocityFormat();

		void GetHalton(float& proj0,float& proj1, uint32_t width, uint32_t height);
		
		void SetHistViewProj(DirectX::XMMATRIX& histViewProj);
		DirectX::XMMATRIX GetHistViewProj();
	protected:
		void InitResources();

		void InitHalton();
		float RadicalInversion(int base, int num);

		ID3D12Device* mDevice;

		uint32_t mRenderTargetWidth;
		uint32_t mRenderTargetHeight;

		unique_ptr<DefaultBuffer> mHistFrameMap;
		unique_ptr<DefaultBuffer> mCurrFrameMap;
		unique_ptr<DefaultBuffer> mVelocityMap;

		DXGI_FORMAT mVelocityMapFormat = DXGI_FORMAT_R16G16_SNORM;
		DXGI_FORMAT mFrameFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

		uint32_t mNumSrvs = 4;
		uint32_t mNumRtvs = 2;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mDepthMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mDepthMapGpuSrv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mHistFrameMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mHistFrameMapGpuSrv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mCurrFrameMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mCurrFrameMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mCurrFrameMapCpuRtv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mVelocityMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mVelocityMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mVelocityMapCpuRtv;

		DirectX::XMFLOAT2 mHalton[8];
		DirectX::XMMATRIX mHistViewProj;
	};
}


