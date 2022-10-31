#pragma once
#include "../Utils/Common.h"
#include "d3dx12.h"
#include <d3d12.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

namespace Carol
{
	class Device;

	class DescriptorHeap
	{
	public:
		ID3D12DescriptorHeap* Get();
		ID3D12DescriptorHeap** GetAddressOf();
		uint32_t GetNumDescriptors();
		uint32_t GetDescriptorSize();
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuDescriptor(uint32_t location);
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuDescriptor(uint32_t location);
	protected:
		ComPtr<ID3D12DescriptorHeap> mDescriptorHeap;
		uint32_t mNumDescriptors;
		uint32_t mDescriptorSize;
	};

	class RtvDescriptorHeap : public DescriptorHeap
	{
	public:
		RtvDescriptorHeap() = default;
		RtvDescriptorHeap(const RtvDescriptorHeap&) = delete;
		RtvDescriptorHeap(RtvDescriptorHeap&&) = delete;
		RtvDescriptorHeap& operator=(const RtvDescriptorHeap&) = delete;

	public:
		void InitRtvDescriptorHeap(
			ID3D12Device* device,
			uint32_t numDescriptors,
			D3D12_DESCRIPTOR_HEAP_FLAGS flag = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
			uint32_t nodeMask = 0);
	};

	class DsvDescriptorHeap : public DescriptorHeap
	{
	public:
		DsvDescriptorHeap() = default;
		DsvDescriptorHeap(const DsvDescriptorHeap&) = delete;
		DsvDescriptorHeap(DsvDescriptorHeap&&) = delete;
		DsvDescriptorHeap& operator=(const DsvDescriptorHeap&) = delete;

	public:
		void InitDsvDescriptorHeap(
			ID3D12Device* device,
			uint32_t numDescriptors,
			D3D12_DESCRIPTOR_HEAP_FLAGS flag = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
			uint32_t nodeMask = 0);
	};

	class CbvSrvUavDescriptorHeap : public DescriptorHeap
	{
	public:
		CbvSrvUavDescriptorHeap() = default;
		CbvSrvUavDescriptorHeap(const CbvSrvUavDescriptorHeap&) = delete;
		CbvSrvUavDescriptorHeap(CbvSrvUavDescriptorHeap&&) = delete;
		CbvSrvUavDescriptorHeap& operator=(const CbvSrvUavDescriptorHeap&) = delete;

	public:
		void InitCbvSrvUavDescriptorHeap(
			ID3D12Device* device,
			uint32_t numDescriptors,
			D3D12_DESCRIPTOR_HEAP_FLAGS flag = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			uint32_t nodeMask = 0);
	};
}




