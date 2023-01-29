#pragma once
#include <utils/d3dx12.h>
#include <utils/common.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>
#include <span>

namespace Carol
{
	class Heap;
	class HeapAllocInfo;
	class DescriptorManager;
	class DescriptorAllocInfo;

	DXGI_FORMAT GetBaseFormat(DXGI_FORMAT format);
	DXGI_FORMAT GetUavFormat(DXGI_FORMAT format);
	DXGI_FORMAT GetDsvFormat(DXGI_FORMAT format);
	uint32_t GetPlaneSize(DXGI_FORMAT format);

	class Resource
	{
	public:
		Resource();
		Resource(
			D3D12_RESOURCE_DESC* desc,
			Heap* heap,
			D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_COMMON,
			D3D12_CLEAR_VALUE* optimizedClearValue = nullptr);
		virtual ~Resource();

		ID3D12Resource* Get()const;
		ID3D12Resource** GetAddressOf();

		byte* GetMappedData()const;
		Heap* GetHeap()const;
		D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress()const;

		void CopySubresources(
			Heap* intermediateHeap,
			const void* data,
			uint32_t byteSize,
			D3D12_RESOURCE_STATES beforeState
		);

		void CopySubresources(
			Heap* intermediateHeap,
			D3D12_SUBRESOURCE_DATA* subresources,
			uint32_t firstSubresource,
			uint32_t numSubresources,
			D3D12_RESOURCE_STATES beforeState
		);

		void CopyData(const void* data, uint32_t byteSize, uint32_t offset = 0);
		void ReleaseIntermediateBuffer();

	protected:
		Microsoft::WRL::ComPtr<ID3D12Resource> mResource;
		std::unique_ptr<HeapAllocInfo> mHeapAllocInfo;

		Microsoft::WRL::ComPtr<ID3D12Resource> mIntermediateBuffer;
		std::unique_ptr<HeapAllocInfo> mIntermediateBufferAllocInfo;

		byte* mMappedData = nullptr;
	};

	class Buffer
	{
	public:
		Buffer();
		Buffer(Buffer&& buffer);
		Buffer& operator=(Buffer&& buffer);
		~Buffer();

		ID3D12Resource* Get()const;
		ID3D12Resource** GetAddressOf()const;
		D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress()const;

		void CopySubresources(
			Heap* intermediateHeap,
			const void* data,
			uint32_t byteSize,
			D3D12_RESOURCE_STATES beforeState
		);

		void CopySubresources(
			Heap* intermediateHeap,
			D3D12_SUBRESOURCE_DATA* subresources,
			uint32_t firstSubresource,
			uint32_t numSubresources,
			D3D12_RESOURCE_STATES beforeState
		);

		void CopyData(const void* data, uint32_t byteSize, uint32_t offset = 0);
		void ReleaseIntermediateBuffer();

		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuSrv(uint32_t planeSlice = 0)const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetShaderCpuSrv(uint32_t planeSlice = 0)const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuSrv(uint32_t planeSlice = 0)const;
		uint32_t GetGpuSrvIdx(uint32_t planeSlice = 0)const;

		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuUav(uint32_t mipSlice = 0, uint32_t planeSlice = 0)const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetShaderCpuUav(uint32_t mipSlice = 0, uint32_t planeSlice = 0)const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuUav(uint32_t mipSlice = 0, uint32_t planeSlice = 0)const;
		uint32_t GetGpuUavIdx(uint32_t mipSlice = 0, uint32_t planeSlice = 0)const;

		D3D12_CPU_DESCRIPTOR_HANDLE GetRtv(uint32_t mipSlice = 0, uint32_t planeSlice = 0)const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetDsv(uint32_t mipSlice = 0)const;

	protected:
		void BindDescriptors();

		virtual void BindSrv() = 0;
		virtual void BindUav() = 0;
		virtual void BindRtv() = 0;
		virtual void BindDsv() = 0;

		virtual void CreateCbvs(std::span<const D3D12_CONSTANT_BUFFER_VIEW_DESC> cbvDesc);
		virtual void CreateSrvs(std::span<const D3D12_SHADER_RESOURCE_VIEW_DESC> srvDesc);
		virtual void CreateUavs(std::span<const D3D12_UNORDERED_ACCESS_VIEW_DESC> uavDesc, bool counter);
		virtual void CreateRtvs(std::span<const D3D12_RENDER_TARGET_VIEW_DESC> rtvDesc);
		virtual void CreateDsvs(std::span<const D3D12_DEPTH_STENCIL_VIEW_DESC> dsvDesc);

		std::unique_ptr<Resource> mResource;
		D3D12_RESOURCE_DESC mResourceDesc = {};

		std::unique_ptr<DescriptorAllocInfo> mCpuCbvAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mGpuCbvAllocInfo;

		std::unique_ptr<DescriptorAllocInfo> mCpuSrvAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mGpuSrvAllocInfo;
		
		std::unique_ptr<DescriptorAllocInfo> mCpuUavAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mGpuUavAllocInfo;

		std::unique_ptr<DescriptorAllocInfo> mRtvAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mDsvAllocInfo;
	};

	enum ColorBufferViewDimension
	{
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE1D,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE1DARRAY,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DARRAY,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DMS,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DMSARRAY,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE3D,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURECUBE,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURECUBEARRAY,
		COLOR_BUFFER_VIEW_DIMENSION_UNKNOWN
	};

	class ColorBuffer : public Buffer
	{
	public:
		ColorBuffer(
			uint32_t width,
			uint32_t height,
			uint32_t depthOrArraySize,
			ColorBufferViewDimension viewDimension,
			DXGI_FORMAT format,
			Heap* heap,
			D3D12_RESOURCE_STATES initState,
			D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
			D3D12_CLEAR_VALUE* optClearValue = nullptr,
			uint32_t mipLevels = 1,
			uint32_t viewMipLevles = 0,
			uint32_t mostDetailedMip = 0,
			float resourceMinLODClamp = 0.f,
			uint32_t viewArraySize = 0,
			uint32_t firstArraySlice = 0,
			uint32_t sampleCount = 1,
			uint32_t sampleQuality = 0);
		ColorBuffer(ColorBuffer&& colorBuffer);
		ColorBuffer& operator=(ColorBuffer&& colorBuffer);

	protected:
		virtual void BindSrv()override;
		virtual void BindUav()override;
		virtual void BindRtv()override;
		virtual void BindDsv()override;

		D3D12_RESOURCE_DIMENSION GetResourceDimension(ColorBufferViewDimension viewDimension)const;

		ColorBufferViewDimension mViewDimension;
		DXGI_FORMAT mFormat;

		uint32_t mViewMipLevels = 1;
		uint32_t mMostDetailedMip = 0;
		float mResourceMinLODClamp = 0.f;

		uint32_t mViewArraySize = 0;
		uint32_t mFirstArraySlice = 0;
		uint32_t mPlaneSize = 0;
	};

	class StructuredBuffer : public Buffer
	{
	public:
		StructuredBuffer(
			uint32_t numElements,
			uint32_t elementSize,
			Heap* heap,
			D3D12_RESOURCE_STATES initState,
			D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
			bool isConstant = false,
			uint32_t viewNumElements = 0,
			uint32_t firstElement = 0);
		StructuredBuffer(StructuredBuffer&& structuredBuffer);
		StructuredBuffer& operator=(StructuredBuffer&& structuredBuffer);

		static void InitCounterResetBuffer(Heap* heapManager);
		void ResetCounter();
		void CopyElements(const void* data, uint32_t offset = 0, uint32_t numElements = 1);

		uint32_t GetNumElements()const;
		uint32_t GetElementSize()const;
		uint32_t GetCounterOffset()const;

		D3D12_GPU_VIRTUAL_ADDRESS GetElementAddress(uint32_t offset)const;
		bool IsConstant()const;

	protected:
		virtual void BindSrv()override;
		virtual void BindUav()override;
		virtual void BindRtv()override;
		virtual void BindDsv()override;

		uint32_t AlignForConstantBuffer(uint32_t byteSize)const;
		uint32_t AlignForUavCounter(uint32_t byteSize)const;

		uint32_t mNumElements = 0;
		uint32_t mElementSize = 0;
		bool mIsConstant = false;

		uint32_t mCounterOffset = 0;
		uint32_t mViewNumElements = 0;
		uint32_t mFirstElement = 0;

		static std::unique_ptr<Resource> sCounterResetBuffer;
	};

	class FastConstantBufferAllocator
	{
	public:
		FastConstantBufferAllocator(
			uint32_t numElements,
			uint32_t elementSize,
			Heap* heap);
		FastConstantBufferAllocator(FastConstantBufferAllocator&& fastResourceAllocator);
		FastConstantBufferAllocator& operator=(FastConstantBufferAllocator&& fastResourceAllocator);

		D3D12_GPU_VIRTUAL_ADDRESS Allocate(const void* data);

	protected:
		std::unique_ptr<StructuredBuffer> mResourceQueue;
		uint32_t mCurrOffset;
	};
	
	class RawBuffer : public Buffer
	{
	public:
		RawBuffer(
			uint32_t byteSize,
			Heap* heap,
			D3D12_RESOURCE_STATES initState,
			D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE
		);
		RawBuffer(RawBuffer&& rawBuffer);
		RawBuffer& operator=(RawBuffer&& rawBuffer);

	protected:
		virtual void BindSrv()override;
		virtual void BindUav()override;
		virtual void BindRtv()override;
		virtual void BindDsv()override;

		uint32_t AlignForRawBuffer(uint32_t byteSize)const;
	};
}
