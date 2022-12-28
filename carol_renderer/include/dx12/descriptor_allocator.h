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
			ID3D12Device* device,
			D3D12_DESCRIPTOR_HEAP_TYPE type, 
			uint32_t initNumCpuDescriptors = 2048);
		virtual ~DescriptorAllocator();
		
		bool CpuAllocate(uint32_t numDescriptors, DescriptorAllocInfo* info);
		bool CpuDeallocate(DescriptorAllocInfo* info);
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(DescriptorAllocInfo* info);
		uint32_t GetDescriptorSize();

	protected:
		void AddCpuDescriptorHeap();

		std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> mCpuDescriptorHeaps;
		std::vector<std::unique_ptr<Buddy>> mCpuBuddies;
		uint32_t mNumCpuDescriptorsPerHeap;

		ID3D12Device* mDevice;
		D3D12_DESCRIPTOR_HEAP_TYPE mType;
		uint32_t mDescriptorSize;
	};

	class RtvDescriptorAllocator : public DescriptorAllocator
	{
	public:
		RtvDescriptorAllocator(
			ID3D12Device* device, 
			uint32_t initNumCpuDescriptors = 2048);
	};

	class DsvDescriptorAllocator : public DescriptorAllocator
	{
	public:
		DsvDescriptorAllocator(
			ID3D12Device* device, 
			uint32_t initNumCpuDescriptors = 2048);
	};

	class CbvSrvUavDescriptorAllocator : public DescriptorAllocator
	{
	public:
		CbvSrvUavDescriptorAllocator(
			ID3D12Device* device,
			uint32_t initNumCpuDescriptors = 2048, 
			uint32_t initNumGpuDescriptors = 2048);
		~CbvSrvUavDescriptorAllocator();
			
		void DelayedDelete(uint32_t currFrame);
		bool GpuAllocate(uint32_t numDescriptors, DescriptorAllocInfo* info);
		bool GpuDeallocate(DescriptorAllocInfo* info);
		ID3D12DescriptorHeap* GetGpuDescriptorHeap();

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetShaderCpuHandle(DescriptorAllocInfo* info);
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetShaderGpuHandle(DescriptorAllocInfo* info);
	
	protected:
		bool GpuDelete(DescriptorAllocInfo* info);
		void ExpandGpuDescriptorHeap();

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mGpuDescriptorHeap;
		std::vector<std::unique_ptr<Buddy>> mGpuBuddies;
		uint32_t mNumGpuDescriptorsPerSection;
		uint32_t mNumGpuDescriptors = 0;

		uint32_t mCurrFrame;
		std::vector<std::vector<DescriptorAllocInfo*>> mGpuDeletionInfo;
	};
}
