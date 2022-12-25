#pragma once
#include <utils/d3dx12.h>
#include <utils/common.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>

namespace Carol
{
	class Heap;
	class HeapAllocInfo;

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

		D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress();

	protected:
		Microsoft::WRL::ComPtr<ID3D12Resource> mResource;
		std::unique_ptr<HeapAllocInfo> mAllocInfo;

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
			ID3D12GraphicsCommandList* cmdList,
			Heap* heap,
			D3D12_SUBRESOURCE_DATA* subresourceData,
			uint32_t firstSubresource = 0,
			uint32_t numSubresources = 1
		);

		void ReleaseIntermediateBuffer();

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> mIntermediateBuffer;
		std::unique_ptr<HeapAllocInfo> mIntermediateBufferAllocInfo;
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
