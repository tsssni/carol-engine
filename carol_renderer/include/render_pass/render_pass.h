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

	protected:
		virtual void InitShaders() = 0;
		virtual void InitPSOs() = 0;
		virtual void InitResources() = 0;
		virtual void InitDescriptors() = 0;
		virtual void CopyDescriptors();
		virtual void DeallocateDescriptors();

		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtv(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsv(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuCbvSrvUav(int idx);
		virtual CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuCbvSrvUav(int idx);

		GlobalResources* mGlobalResources;
		std::unique_ptr<DescriptorAllocInfo> mCpuCbvSrvUavAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mGpuCbvSrvUavAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mRtvAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mDsvAllocInfo;
	};
}
