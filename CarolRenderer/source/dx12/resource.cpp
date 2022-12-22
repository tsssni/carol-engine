#include <dx12/resource.h>
#include <dx12/Heap.h>
#include <d3dcompiler.h>

namespace Carol {
	using std::make_unique;
}

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

Carol::Blob::Blob(const void* data, uint32_t size)
{
	ThrowIfFailed(D3DCreateBlob(size, mBlob.GetAddressOf()));
	CopyMemory(mBlob->GetBufferPointer(), data, size);
}

ID3D12Resource* Carol::Resource::operator->()
{
	return mResource.Get();
}

Carol::Resource::Resource()
{
}

Carol::Resource::Resource(D3D12_RESOURCE_DESC* desc, Heap* heap, D3D12_RESOURCE_STATES initState, D3D12_CLEAR_VALUE* optimizedClearValue)
{
	mAllocInfo = make_unique<HeapAllocInfo>();
	heap->CreateResource(&mResource, desc, mAllocInfo.get(), initState, optimizedClearValue);
}

Carol::Resource::~Resource()
{
	if (mAllocInfo)
	{
		mAllocInfo->Heap->DeleteResource(mAllocInfo.get());
	}
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

Carol::DefaultResource::DefaultResource(D3D12_RESOURCE_DESC* desc, Heap* heap, D3D12_RESOURCE_STATES initState, D3D12_CLEAR_VALUE* optimizedClearValue)
	:Resource(desc,heap,initState,optimizedClearValue)
{
	mIntermediateBufferAllocInfo = make_unique<HeapAllocInfo>();
}

void Carol::DefaultResource::CopySubresources(ID3D12GraphicsCommandList* cmdList, Heap* heap, D3D12_SUBRESOURCE_DATA* subresourceData, uint32_t firstSubresource, uint32_t numSubresources)
{
	cmdList->ResourceBarrier(1,GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mResource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST)));
	auto resourceSize = GetRequiredIntermediateSize(mResource.Get(), firstSubresource, numSubresources);
	heap->CreateResource(&mIntermediateBuffer, GetRvaluePtr(CD3DX12_RESOURCE_DESC::Buffer(resourceSize)), mIntermediateBufferAllocInfo.get(), D3D12_RESOURCE_STATE_GENERIC_READ);
	
	UpdateSubresources(cmdList, mResource.Get(), mIntermediateBuffer.Get(), 0, firstSubresource, numSubresources, subresourceData);
	cmdList->ResourceBarrier(1,GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ)));
}

void Carol::DefaultResource::ReleaseIntermediateBuffer()
{
	if (mIntermediateBuffer)
	{
		mIntermediateBufferAllocInfo->Heap->DeleteResource(mIntermediateBufferAllocInfo.get());
		mIntermediateBuffer.Reset();
	}
}

D3D12_SUBRESOURCE_DATA Carol::CreateSingleSubresource(const void* initData, uint32_t byteSize)
{
	D3D12_SUBRESOURCE_DATA subresource;
	subresource.RowPitch = byteSize;
	subresource.SlicePitch = subresource.RowPitch;
	subresource.pData = initData;

	return subresource;
}

Carol::UploadResource::UploadResource(D3D12_RESOURCE_DESC* desc, Heap* heap, D3D12_RESOURCE_STATES initState, D3D12_CLEAR_VALUE* optimizedClearValue)
	:Resource(desc,heap,initState,optimizedClearValue)
{
}

Carol::ReadbackResource::ReadbackResource(D3D12_RESOURCE_DESC* desc, Heap* heap, D3D12_RESOURCE_STATES initState, D3D12_CLEAR_VALUE* optimizedClearValue)
	:Resource(desc,heap,initState,optimizedClearValue)
{
}


