#pragma once
#include "../Utils/Common.h"
#include "d3dx12.h"
#include <d3d12.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

namespace Carol
{
	class GraphicsCommandList;
	class Device;

	class Blob
	{
	public:
		ID3DBlob* operator->();
		ID3DBlob* Get();
		ID3DBlob** GetAddressOf();
		void InitBlob(const void* data, uint32_t size);
	protected:
		ComPtr<ID3DBlob> mBlob;
	};

	class Resource
	{
	public:
		void Reset();
		void Release();
		ID3D12Resource* Get();
		ID3D12Resource** GetAddressOf();
		ID3D12Resource* operator->();
		D3D12_GPU_VIRTUAL_ADDRESS GetVirtualAddress();
		void TransitionBarrier(
			ID3D12GraphicsCommandList* cmdList,
			D3D12_RESOURCE_STATES stateBefore,
			D3D12_RESOURCE_STATES stateAfter);
	protected:
		ComPtr<ID3D12Resource> mResource;
	};

	class DefaultBuffer : public Resource
	{
	public:
		void InitDefaultBuffer(
			ID3D12Device* device,
			D3D12_HEAP_FLAGS heapFlag,
			D3D12_RESOURCE_DESC* bufDesc,
			D3D12_CLEAR_VALUE* optimizedClearValue = nullptr);

		void CopySubresources(
			ID3D12GraphicsCommandList* cmdList,
			D3D12_SUBRESOURCE_DATA* subresourcesData,
			uint32_t firstSubresource = 0,
			uint32_t numSubresources = 1);

	private:
		ComPtr<ID3D12Resource> mIntermediateBuffer;
	};

	class ReadBackBuffer : public Resource
	{
	public:
		void InitReadBackBuffer(
			ID3D12Device* device,
			D3D12_HEAP_FLAGS heapFlag,
			D3D12_RESOURCE_DESC* bufDesc,
			D3D12_CLEAR_VALUE* optimizedClearValue = nullptr);
	};

	template<typename T>
	class UploadBuffer :public Resource
	{
	public:
		~UploadBuffer()
		{
			if (mResource.Get())
			{
				mResource->Unmap(0, nullptr);
			}

			mMappedData = nullptr;
		}
	public:
		void InitUploadBuffer(
			ID3D12Device* device,
			D3D12_HEAP_FLAGS heapFlag,
			uint32_t elementCount,
			bool isConstant,
			D3D12_CLEAR_VALUE* optimizedClearValue = nullptr)
		{
			if (elementCount == 0)
			{
				return;
			}

			mResource = nullptr;

			mElementByteSize = sizeof(T);
			if (isConstant)
			{
				mElementByteSize = CalcConstantElementSize(mElementByteSize);
			}

			ThrowIfFailed(device->CreateCommittedResource(
				GetRvaluePtr(CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)),
				heapFlag,
				GetRvaluePtr(CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize * elementCount)),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				optimizedClearValue,
				IID_PPV_ARGS(mResource.GetAddressOf())));

			ThrowIfFailed(mResource->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));
		}

		void CopyData(int elementIndex, const T& data)
		{
			memcpy(&mMappedData[elementIndex * mElementByteSize], &data, sizeof(T));
		}

		uint32_t CalcConstantElementSize(uint32_t elementByteSize)
		{
			return (~255) & (elementByteSize + 255);
		}
	private:
		uint32_t mElementByteSize;
		byte* mMappedData;
	};

	D3D12_SUBRESOURCE_DATA CreateSingleSubresource(const void* initData, uint32_t byteSize);
}
