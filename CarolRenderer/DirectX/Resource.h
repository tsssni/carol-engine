#pragma once
#include "../Utils/d3dx12.h"
#include "../Utils/Common.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>

using Microsoft::WRL::ComPtr;
using std::unique_ptr;

namespace Carol
{
	class Heap;
	class HeapAllocInfo;
	class RenderData;

	class Blob
	{
	public:
		Blob() = default;
		Blob(const void* data, uint32_t size);
		ID3DBlob* operator->();
		ID3DBlob* Get();
		ID3DBlob** GetAddressOf();
	protected:
		ComPtr<ID3DBlob> mBlob;
	};

	class Resource
	{
	public:
		Resource();
		Resource(
			D3D12_RESOURCE_DESC* desc,
			Heap* heap,
			D3D12_RESOURCE_STATES initState,
			D3D12_CLEAR_VALUE* optimizedClearValue = nullptr);
		~Resource();

		void Reset();
		void Release();
		ID3D12Resource* Get();
		ID3D12Resource** GetAddressOf();
		ID3D12Resource* operator->();
	protected:
		ComPtr<ID3D12Resource> mResource;
		unique_ptr<HeapAllocInfo> mAllocInfo;

	};

	class DefaultResource : public Resource
	{
	public:
		DefaultResource(
			D3D12_RESOURCE_DESC* desc,
			Heap* heap,
			D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_COMMON,
			D3D12_CLEAR_VALUE* optimizedClearValue = nullptr
		);

		void CopySubresources(
			RenderData* renderData,
			D3D12_SUBRESOURCE_DATA* subresourceData,
			uint32_t firstSubresource = 0,
			uint32_t numSubresources = 1
		);

		void ReleaseIntermediateBuffer();

	private:
		ComPtr<ID3D12Resource> mIntermediateBuffer;
		unique_ptr<HeapAllocInfo> mIntermediateBufferAllocInfo;
	};

	class UploadResource : public Resource
	{
	public:
		UploadResource(
			D3D12_RESOURCE_DESC* desc,
			Heap* heap,
			D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_CLEAR_VALUE* optimizedClearValue = nullptr);
	};

	class ReadbackResource : public Resource
	{
	public:
		ReadbackResource(
			D3D12_RESOURCE_DESC* desc,
			Heap* heap,
			D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_CLEAR_VALUE* optimizedClearValue = nullptr);
	};

	D3D12_SUBRESOURCE_DATA CreateSingleSubresource(const void* initData, uint32_t byteSize);
}
