#pragma once
#include <utils/d3dx12.h>
#include <wrl/client.h>
#include <memory>

namespace Carol
{
	class GlobalResources;
	class DescriptorAllocInfo;

	class Pass
	{
	public:
		Pass(GlobalResources* globalResources);
		Pass(const Pass&) = delete;
		Pass(Pass&&) = delete;
		Pass& operator=(const Pass&) = delete;
		~Pass();

		virtual void Draw() = 0;
		virtual void Update() = 0;
		virtual void OnResize();
		virtual void ReleaseIntermediateBuffers() = 0;

	private:
		void CopyCbvDescriptors();
		void CopySrvDescriptors();
		void CopyUavDescriptors();

	protected:
		virtual void CopyDescriptors();
		virtual void InitShaders() = 0;
		virtual void InitPSOs() = 0;
		virtual void InitResources() = 0;
		virtual void InitDescriptors() = 0;

		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtv(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsv(int idx);

		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetTex1DSrv(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetTex2DSrv(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetTex3DSrv(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetTexCubeSrv(int idx);

		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuCbv(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetShaderCpuCbv(int idx);
		virtual CD3DX12_GPU_DESCRIPTOR_HANDLE GetShaderGpuCbv(int idx);

		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuSrv(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetShaderCpuSrv(int idx);
		virtual CD3DX12_GPU_DESCRIPTOR_HANDLE GetShaderGpuSrv(int idx);

		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuUav(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetShaderCpuUav(int idx);
		virtual CD3DX12_GPU_DESCRIPTOR_HANDLE GetShaderGpuUav(int idx);

		GlobalResources* mGlobalResources;

		uint32_t mTex1DGpuSrvStartOffset = 0;
		uint32_t mTex2DGpuSrvStartOffset = 0;
		uint32_t mTex3DGpuSrvStartOffset = 0;
		uint32_t mTexCubeGpuSrvStartOffset = 0;

		std::unique_ptr<DescriptorAllocInfo> mCpuCbvAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mGpuCbvAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mCpuSrvAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mGpuSrvAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mCpuUavAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mGpuUavAllocInfo;

		std::unique_ptr<DescriptorAllocInfo> mTex1DSrvAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mTex2DSrvAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mTex3DSrvAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mTexCubeSrvAllocInfo;

		std::unique_ptr<DescriptorAllocInfo> mRtvAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mDsvAllocInfo;
	};
}
