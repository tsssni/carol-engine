#pragma once
#include "../DirectX/Buffer.h"
#include "../FrameResources.h"
#include "../DirectX/d3dx12.h"
#include <memory>
#include <vector>

using std::unique_ptr;
using std::vector;

namespace Carol
{
	class PipelineState;

	class SsaoManager
	{
	public:
		void InitSsaoManager(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, uint32_t width, uint32_t height);
		void OnResize(uint32_t width,uint32_t height);
		void RebuildDescriptors(Resource* depthStencilBuffer);
		
		void SetPSOs(ID3D12PipelineState* ssaoPso, ID3D12PipelineState* ssaoBlurPso);
		void ComputeSsao(
			ID3D12GraphicsCommandList* cmdList,
			ID3D12RootSignature* rootSignature,
			FrameResource* frameResource,
			uint32_t blurCount);

		vector<float> CalcGaussWeights(float sigma);

		void InitDescriptors(
			Resource* depthStencilBuffer,
			CD3DX12_CPU_DESCRIPTOR_HANDLE cpuSrv,
			CD3DX12_GPU_DESCRIPTOR_HANDLE gpuSrv,
			CD3DX12_CPU_DESCRIPTOR_HANDLE cpuRtv,
			uint32_t cbvSrvUavDescriptorSize,
			uint32_t rtvDescriptorSize);

		DefaultBuffer* GetNormalMap();
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetNormalMapRtv();
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetSsaoMapSrv();

		uint32_t GetNumSrvs();
		uint32_t GetNumRtvs();
		DXGI_FORMAT GetNormalMapFormat();
		DXGI_FORMAT GetAmbientMapFormat();

		void GetOffsetVectors(DirectX::XMFLOAT4 offsets[14]);

	protected:
		void InitResources();
		void InitRandomVectors();
		void InitRandomVectorMap(ID3D12GraphicsCommandList* cmdList);

		void BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, FrameResource* frameResource, uint32_t blurCount);
		void BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, bool horzBlur);

		ID3D12Device* mDevice;

		uint32_t mRenderTargetWidth = 0;
		uint32_t mRenderTargetHeight = 0;
		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;

		ID3D12PipelineState* mSsaoPso;
		ID3D12PipelineState* mSsaoBlurPso;

		unique_ptr<DefaultBuffer> mNormalMap;
		unique_ptr<DefaultBuffer> mRandomVecMap;
		unique_ptr<DefaultBuffer> mAmbientMap0;
		unique_ptr<DefaultBuffer> mAmbientMap1;

		uint32_t mNumSrvs = 5;
		uint32_t mNumRtvs = 3;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mNormalMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mNormalMapGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mNormalMapCpuRtv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mDepthMapCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mDepthMapGpuSrv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mRandomVecCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mRandomVecGpuSrv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mAmbientMap0CpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mAmbientMap0GpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mAmbientMap0CpuRtv;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mAmbientMap1CpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mAmbientMap1GpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mAmbientMap1CpuRtv;

		DXGI_FORMAT mNormalMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		DXGI_FORMAT mAmbientMapFormat = DXGI_FORMAT_R16_UNORM;

		DirectX::XMFLOAT4 mOffsets[14];
	};
}

