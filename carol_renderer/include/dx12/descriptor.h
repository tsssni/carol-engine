#pragma once
#include <utils/d3dx12.h>
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
		uint32_t StartOffset = 0;
		uint32_t NumDescriptors = 0;
	};

	class DescriptorAllocator
	{
	public:
		DescriptorAllocator(
			D3D12_DESCRIPTOR_HEAP_TYPE type, 
			uint32_t initNumCpuDescriptors = 2048,
			uint32_t numGpuDescriptors = 65536);
		virtual ~DescriptorAllocator();
		
		std::unique_ptr<DescriptorAllocInfo> CpuAllocate(uint32_t numDescriptors);
		void CpuDeallocate(std::unique_ptr<DescriptorAllocInfo> info);
		std::unique_ptr<DescriptorAllocInfo> GpuAllocate(uint32_t numDescriptors);
		void GpuDeallocate(std::unique_ptr<DescriptorAllocInfo> info);
		void DelayedDelete();

		ID3D12DescriptorHeap* GetGpuDescriptorHeap()const;
		uint32_t GetDescriptorSize()const;

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(const DescriptorAllocInfo* info, uint32_t offset = 0)const;
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetShaderCpuHandle(const DescriptorAllocInfo* info, uint32_t offset = 0)const;
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(const DescriptorAllocInfo* info, uint32_t offset = 0)const;

	protected:
		void AddCpuDescriptorHeap();
		void InitGpuDescriptorHeap();

		void CpuDelete(std::unique_ptr<DescriptorAllocInfo> info);
		void GpuDelete(std::unique_ptr<DescriptorAllocInfo> info);

		std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> mCpuDescriptorHeaps;
		std::vector<std::unique_ptr<Buddy>> mCpuBuddies;
		uint32_t mNumCpuDescriptorsPerHeap = 0;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mGpuDescriptorHeap;
		std::unique_ptr<Buddy> mGpuBuddy;
		uint32_t mNumGpuDescriptors = 0;

		std::vector<std::vector<std::unique_ptr<DescriptorAllocInfo>>> mCpuDeletedAllocInfo;
		std::vector<std::vector<std::unique_ptr<DescriptorAllocInfo>>> mGpuDeletedAllocInfo;

		D3D12_DESCRIPTOR_HEAP_TYPE mType;
		uint32_t mDescriptorSize = 0;
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

		std::unique_ptr<DescriptorAllocInfo> CpuCbvSrvUavAllocate(uint32_t numDescriptors);
		std::unique_ptr<DescriptorAllocInfo> GpuCbvSrvUavAllocate(uint32_t numDescriptors);
		std::unique_ptr<DescriptorAllocInfo> RtvAllocate(uint32_t numDescriptors);
		std::unique_ptr<DescriptorAllocInfo> DsvAllocate(uint32_t numDescriptors);

		void CpuCbvSrvUavDeallocate(std::unique_ptr<DescriptorAllocInfo> info);
		void GpuCbvSrvUavDeallocate(std::unique_ptr<DescriptorAllocInfo> info);
		void RtvDeallocate(std::unique_ptr<DescriptorAllocInfo> info);
		void DsvDeallocate(std::unique_ptr<DescriptorAllocInfo> info);	
		void DelayedDelete();

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuCbvSrvUavHandle(const DescriptorAllocInfo* info, uint32_t offset = 0)const;
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetShaderCpuCbvSrvUavHandle(const DescriptorAllocInfo* info, uint32_t offset = 0)const;
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuCbvSrvUavHandle(const DescriptorAllocInfo* info, uint32_t offset = 0)const;
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtvHandle(const DescriptorAllocInfo* info, uint32_t offset = 0)const;
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsvHandle(const DescriptorAllocInfo* info, uint32_t offset = 0)const;

		uint32_t GetCbvSrvUavSize()const;
		uint32_t GetRtvSize()const;
		uint32_t GetDsvSize()const;

		ID3D12DescriptorHeap* GetResourceDescriptorHeap()const;
	protected:
		std::unique_ptr<DescriptorAllocator> mCbvSrvUavAllocator;
		std::unique_ptr<DescriptorAllocator> mRtvAllocator;
		std::unique_ptr<DescriptorAllocator> mDsvAllocator;
	};
}
