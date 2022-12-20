#pragma once
#include <utils/d3dx12.h>
#include <wrl/client.h>
#include <memory>

namespace Carol
{
	class GlobalResources;
	class DescriptorAllocInfo;

	class Manager
	{
	public:
		Manager(GlobalResources* globalResources);
		Manager(const Manager&) = delete;
		Manager(Manager&&) = delete;
		Manager& operator=(const Manager&) = delete;
		~Manager();

		virtual void Draw() = 0;
		virtual void Update() = 0;
		virtual void OnResize();
		virtual void ReleaseIntermediateBuffers() = 0;
		virtual void CopyDescriptors();
	protected:
		virtual void InitRootSignature() = 0;
		virtual void InitShaders() = 0;
		virtual void InitPSOs() = 0;
		virtual void InitResources() = 0;
		virtual void InitDescriptors() = 0;

		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtv(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsv(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetCbv(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuSrv(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetShaderCpuSrv(int idx);
		virtual CD3DX12_GPU_DESCRIPTOR_HANDLE GetShaderGpuSrv(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuUav(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetShaderCpuUav(int idx);
		virtual CD3DX12_GPU_DESCRIPTOR_HANDLE GetShaderGpuUav(int idx);

		GlobalResources* mGlobalResources;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;

		std::unique_ptr<DescriptorAllocInfo> mCpuCbvSrvUavAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mGpuCbvSrvUavAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mRtvAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mDsvAllocInfo;
	};
}
