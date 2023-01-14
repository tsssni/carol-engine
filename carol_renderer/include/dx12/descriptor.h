#pragma once
#include <utils/d3dx12.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <memory>
#include <unordered_map>

namespace Carol
{
	class Buddy;
	class Resource;
	class DescriptorAllocator;

	class DescriptorAllocInfo
	{
	public:
		DescriptorAllocator* Allocator;
		uint32_t StartOffset = 0;
		uint32_t NumDescriptors = 0;
	};

	class DescriptorAllocator
	{
	public:
		DescriptorAllocator(
			D3D12_DESCRIPTOR_HEAP_TYPE type, 
			uint32_t initNumCpuDescriptors = 2048,
			uint32_t initNumGpuDescriptors = 2048);
		virtual ~DescriptorAllocator();
		
		bool CpuAllocate(uint32_t numDescriptors, DescriptorAllocInfo* info);
		bool CpuDeallocate(DescriptorAllocInfo* info);
		bool GpuAllocate(uint32_t numDescriptors, DescriptorAllocInfo* info);
		bool GpuDeallocate(DescriptorAllocInfo* info);
		void DelayedDelete();

		void CreateCbv(D3D12_CONSTANT_BUFFER_VIEW_DESC* cbvDesc, DescriptorAllocInfo* info, uint32_t offset = 0);
		void CreateSrv(D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc, Resource* resource, DescriptorAllocInfo* info, uint32_t offset = 0);
		void CreateUav(D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc, Resource* resource, Resource* counterResource, DescriptorAllocInfo* info, uint32_t offset = 0);
		void CreateRtv(D3D12_RENDER_TARGET_VIEW_DESC* rtvDesc, Resource* resource, DescriptorAllocInfo* info, uint32_t offset = 0);
		void CreateDsv(D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDesc, Resource* resource, DescriptorAllocInfo* info, uint32_t offset = 0);

		ID3D12DescriptorHeap* GetGpuDescriptorHeap();
		uint32_t GetDescriptorSize();

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(DescriptorAllocInfo* info, uint32_t offset = 0);
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetShaderCpuHandle(DescriptorAllocInfo* info, uint32_t offset = 0);
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(DescriptorAllocInfo* info, uint32_t offset = 0);
		void CopyDescriptors(DescriptorAllocInfo* cpuInfo, DescriptorAllocInfo* shaderCpuInfo);

	protected:
		void AddCpuDescriptorHeap();
		void ExpandGpuDescriptorHeap();

		bool CpuDelete(DescriptorAllocInfo* info);
		bool GpuDelete(DescriptorAllocInfo* info);

		std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> mCpuDescriptorHeaps;
		std::vector<std::unique_ptr<Buddy>> mCpuBuddies;
		uint32_t mNumCpuDescriptorsPerHeap;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mGpuDescriptorHeap;
		std::vector<std::unique_ptr<Buddy>> mGpuBuddies;
		uint32_t mNumGpuDescriptorsPerSection;
		uint32_t mNumGpuDescriptors = 0;

		std::vector<std::vector<DescriptorAllocInfo>> mCpuDeletedAllocInfo;
		std::vector<std::vector<DescriptorAllocInfo>> mGpuDeletedAllocInfo;
		uint32_t mCurrFrame;

		D3D12_DESCRIPTOR_HEAP_TYPE mType;
		uint32_t mDescriptorSize;
	};

	class DescriptorManager
	{
	public:
		DescriptorManager(
			uint32_t initCpuCbvSrvUavHeapSize = 2048,
			uint32_t initGpuCbvSrvUavHeapSize = 2048,
			uint32_t initRtvHeapSize = 2048,
			uint32_t initDsvHeapSize = 2048
		);
		DescriptorManager(DescriptorManager&& manager);
		DescriptorManager(const DescriptorManager&) = delete;
		DescriptorManager& operator=(const DescriptorManager&) = delete;

		void CpuCbvSrvUavAllocate(uint32_t numDescriptors, DescriptorAllocInfo* info);
		void GpuCbvSrvUavAllocate(uint32_t numDescriptors, DescriptorAllocInfo* info);
		void RtvAllocate(uint32_t numDescriptors, DescriptorAllocInfo* info);
		void DsvAllocate(uint32_t numDescriptors, DescriptorAllocInfo* info);

		void CpuCbvSrvUavDeallocate(DescriptorAllocInfo* info);
		void GpuCbvSrvUavDeallocate(DescriptorAllocInfo* info);
		void RtvDeallocate(DescriptorAllocInfo* info);
		void DsvDeallocate(DescriptorAllocInfo* info);	
		void DelayedDelete();

		void CreateCbv(D3D12_CONSTANT_BUFFER_VIEW_DESC* cbvDesc, DescriptorAllocInfo* info, uint32_t offset = 0);
		void CreateSrv(D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc, Resource* resource, DescriptorAllocInfo* info, uint32_t offset = 0);
		void CreateUav(D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc, Resource* resource, Resource* counterResource, DescriptorAllocInfo* info, uint32_t offset = 0);
		void CreateRtv(D3D12_RENDER_TARGET_VIEW_DESC* rtvDesc, Resource* resource, DescriptorAllocInfo* info, uint32_t offset = 0);
		void CreateDsv(D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDesc, Resource* resource, DescriptorAllocInfo* info, uint32_t offset = 0);

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuCbvSrvUavHandle(DescriptorAllocInfo* info, uint32_t offset = 0);
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetShaderCpuCbvSrvUavHandle(DescriptorAllocInfo* info, uint32_t offset = 0);
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuCbvSrvUavHandle(DescriptorAllocInfo* info, uint32_t offset = 0);
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtvHandle(DescriptorAllocInfo* info, uint32_t offset = 0);
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsvHandle(DescriptorAllocInfo* info, uint32_t offset = 0);

		uint32_t GetCbvSrvUavSize();
		uint32_t GetRtvSize();
		uint32_t GetDsvSize();

		ID3D12DescriptorHeap* GetResourceDescriptorHeap();
		void CopyCbvSrvUav(DescriptorAllocInfo* cpuInfo, DescriptorAllocInfo* shaderCpuInfo);
	protected:
		std::unique_ptr<DescriptorAllocator> mCbvSrvUavAllocator;
		std::unique_ptr<DescriptorAllocator> mRtvAllocator;
		std::unique_ptr<DescriptorAllocator> mDsvAllocator;
	};
}
