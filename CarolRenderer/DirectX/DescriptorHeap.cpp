#include "DescriptorHeap.h"
#include "../Utils/Common.h"

ID3D12DescriptorHeap* Carol::DescriptorHeap::Get()
{
	return mDescriptorHeap.Get();
}

ID3D12DescriptorHeap** Carol::DescriptorHeap::GetAddressOf()
{
	return mDescriptorHeap.GetAddressOf();
}

uint32_t Carol::DescriptorHeap::GetNumDescriptors()
{
	return mNumDescriptors;
}

uint32_t Carol::DescriptorHeap::GetDescriptorSize()
{
	return mDescriptorSize;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DescriptorHeap::GetCpuDescriptor(uint32_t location)
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), location, mDescriptorSize);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::DescriptorHeap::GetGpuDescriptor(uint32_t location)
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), location, mDescriptorSize);
}

void Carol::RtvDescriptorHeap::InitRtvDescriptorHeap(ID3D12Device* device, uint32_t numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAGS flag, uint32_t nodeMask)
{
	mNumDescriptors = numDescriptors;
	mDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = numDescriptors;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = flag;
	rtvHeapDesc.NodeMask = nodeMask;

	ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mDescriptorHeap.GetAddressOf())));
}

void Carol::DsvDescriptorHeap::InitDsvDescriptorHeap(ID3D12Device* device, uint32_t numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAGS flag, uint32_t nodeMask)
{
	mNumDescriptors = numDescriptors;
	mDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = numDescriptors;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = flag;
	dsvHeapDesc.NodeMask = nodeMask;

	ThrowIfFailed(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDescriptorHeap.GetAddressOf())));
}

void Carol::CbvSrvUavDescriptorHeap::InitCbvSrvUavDescriptorHeap(ID3D12Device* device, uint32_t numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAGS flag, uint32_t nodeMask)
{
	mNumDescriptors = numDescriptors;
	mDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavHeapDesc = {};
	cbvSrvUavHeapDesc.NumDescriptors = numDescriptors;
	cbvSrvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvSrvUavHeapDesc.Flags = flag;
	cbvSrvUavHeapDesc.NodeMask = nodeMask;

	ThrowIfFailed(device->CreateDescriptorHeap(&cbvSrvUavHeapDesc, IID_PPV_ARGS(mDescriptorHeap.GetAddressOf())));
}
