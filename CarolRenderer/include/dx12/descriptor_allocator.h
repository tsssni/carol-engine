#pragma once
#include <utils/d3dx12.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <memory>

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
			uint32_t initNumCpuDescriptors = 256, 
			uint32_t initNumGpuDescriptors = 256);
		
		bool CpuAllocate(uint32_t numDescriptors, DescriptorAllocInfo* info);
		bool CpuDeallocate(DescriptorAllocInfo* info);
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(DescriptorAllocInfo* info);

		bool GpuAllocate(uint32_t numDescriptors, DescriptorAllocInfo* info);
		bool GpuDeallocate(DescriptorAllocInfo* info);
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetShaderCpuHandle(DescriptorAllocInfo* info);
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetShaderGpuHandle(DescriptorAllocInfo* info);

		uint32_t GetDescriptorSize();
		ID3D12DescriptorHeap* GetGpuDescriptorHeap();
	protected:
		void AddCpuDescriptorHeap();
		void ExpandGpuDescriptorHeap();

		std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> mCpuDescriptorHeaps;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mGpuDescriptorHeap;
		std::vector<std::unique_ptr<Buddy>> mCpuBuddies;
		std::vector<std::unique_ptr<Buddy>> mGpuBuddies;

		uint32_t mNumCpuDescriptorsPerHeap;
		uint32_t mNumGpuDescriptorsPerSection;
		uint32_t mNumGpuDescriptors = 0;

		ID3D12Device* mDevice;
		D3D12_DESCRIPTOR_HEAP_TYPE mType;
		uint32_t mDescriptorSize;
	};
}
