#include "Buffer.h"
#include <d3dcompiler.h>

ID3DBlob* Carol::Blob::operator->()
{
	return mBlob.Get();
}

ID3DBlob* Carol::Blob::Get()
{
	return mBlob.Get();
}

ID3DBlob** Carol::Blob::GetAddressOf()
{
	return mBlob.GetAddressOf();
}

void Carol::Blob::InitBlob(const void* data, uint32_t size)
{
	ThrowIfFailed(D3DCreateBlob(size, mBlob.GetAddressOf()));
	CopyMemory(mBlob->GetBufferPointer(), data, size);
}

ID3D12Resource* Carol::Resource::operator->()
{
	return mResource.Get();
}

void Carol::Resource::Reset()
{
	mResource.Reset();
}

void Carol::Resource::Release()
{
	mResource.ReleaseAndGetAddressOf();
}

ID3D12Resource* Carol::Resource::Get()
{
	return mResource.Get();
}

ID3D12Resource** Carol::Resource::GetAddressOf()
{
	return mResource.GetAddressOf();
}

D3D12_GPU_VIRTUAL_ADDRESS Carol::Resource::GetVirtualAddress()
{
	return mResource->GetGPUVirtualAddress();
}

void Carol::Resource::TransitionBarrier(
	ID3D12GraphicsCommandList* cmdList,
	D3D12_RESOURCE_STATES stateBefore,
	D3D12_RESOURCE_STATES stateAfter)
{
	cmdList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mResource.Get(), stateBefore, stateAfter)));
}

void Carol::DefaultBuffer::InitDefaultBuffer(
	ID3D12Device* device,
	D3D12_HEAP_FLAGS heapFlag,
	D3D12_RESOURCE_DESC* bufDesc,
	D3D12_CLEAR_VALUE* optimizedClearValue)
{
	mResource = nullptr;

	ThrowIfFailed(device->CreateCommittedResource(
		GetRvaluePtr(CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)),
		heapFlag,
		bufDesc,
		D3D12_RESOURCE_STATE_COMMON,
		optimizedClearValue,
		IID_PPV_ARGS(mResource.GetAddressOf())));
}

void Carol::DefaultBuffer::CopySubresources(
	ID3D12GraphicsCommandList* cmdList,
	D3D12_SUBRESOURCE_DATA* subresourcesData,
	uint32_t firstSubresource,
	uint32_t numSubresources)
{
	ComPtr<ID3D12Device> device;
	mResource->GetDevice(IID_PPV_ARGS(device.GetAddressOf()));

	TransitionBarrier(cmdList, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
	auto resourceSize = GetRequiredIntermediateSize(mResource.Get(), firstSubresource, numSubresources);

	ThrowIfFailed(device->CreateCommittedResource(
		GetRvaluePtr(CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)),
		D3D12_HEAP_FLAG_NONE,
		GetRvaluePtr(CD3DX12_RESOURCE_DESC::Buffer(resourceSize)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mIntermediateBuffer.GetAddressOf())
	));
	
	UpdateSubresources(cmdList, mResource.Get(), mIntermediateBuffer.Get(), 0, firstSubresource, numSubresources, subresourcesData);
	TransitionBarrier(cmdList, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
}

void Carol::ReadBackBuffer::InitReadBackBuffer(
	ID3D12Device* device,
	D3D12_HEAP_FLAGS heapFlag,
	D3D12_RESOURCE_DESC* bufDesc,
	D3D12_CLEAR_VALUE* optimizedClearValue)
{
	mResource = nullptr;

	ThrowIfFailed(device->CreateCommittedResource(
		GetRvaluePtr(CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK)),
		heapFlag,
		bufDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		optimizedClearValue,
		IID_PPV_ARGS(mResource.GetAddressOf())));
}

D3D12_SUBRESOURCE_DATA Carol::CreateSingleSubresource(const void* initData, uint32_t byteSize)
{
	D3D12_SUBRESOURCE_DATA subresource;
	subresource.RowPitch = byteSize;
	subresource.SlicePitch = subresource.RowPitch;
	subresource.pData = initData;

	return subresource;
}