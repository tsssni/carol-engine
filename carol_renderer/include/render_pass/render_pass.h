#pragma once
#include <utils/d3dx12.h>
#include <wrl/client.h>
#include <memory>

namespace Carol
{
	class GlobalResources;
	class DescriptorAllocInfo;

	class RenderPass
	{
	public:
		RenderPass(GlobalResources* globalResources);
		RenderPass(const RenderPass&) = delete;
		RenderPass(RenderPass&&) = delete;
		RenderPass& operator=(const RenderPass&) = delete;
		~RenderPass();

		virtual void Draw() = 0;
		virtual void Update() = 0;
		virtual void OnResize();
		virtual void ReleaseIntermediateBuffers() = 0;
		virtual void CopyDescriptors();

	private:
		void CopyCbvDescriptors();
		void CopySrvDescriptors();
		void CopyUavDescriptors();

	protected:
		virtual void InitShaders() = 0;
		virtual void InitPSOs() = 0;
		virtual void InitResources() = 0;
		virtual void InitDescriptors() = 0;

		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtv(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsv(int idx);

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

		std::unique_ptr<DescriptorAllocInfo> mCpuCbvAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mCpuSrvAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mCpuUavAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mRtvAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mDsvAllocInfo;

		std::vector<std::unique_ptr<DescriptorAllocInfo>> mGpuCbvAllocInfo;
		std::vector<std::unique_ptr<DescriptorAllocInfo>> mGpuSrvAllocInfo;
		std::vector<std::unique_ptr<DescriptorAllocInfo>> mGpuUavAllocInfo;
	};
}
