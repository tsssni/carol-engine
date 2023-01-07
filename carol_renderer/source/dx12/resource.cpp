#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/descriptor_allocator.h>
#include <vector>

namespace Carol {
	using std::make_unique;
	using std::vector;
}

DXGI_FORMAT Carol::GetBaseFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		return DXGI_FORMAT_R8G8B8A8_TYPELESS;

	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		return DXGI_FORMAT_B8G8R8A8_TYPELESS;

	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		return DXGI_FORMAT_B8G8R8X8_TYPELESS;

	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		return DXGI_FORMAT_R32G8X24_TYPELESS;

	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
		return DXGI_FORMAT_R32_TYPELESS;

	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		return DXGI_FORMAT_R24G8_TYPELESS;

	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
		return DXGI_FORMAT_R16_TYPELESS;

	default:
		return format;
	}
}

DXGI_FORMAT Carol::GetUavFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		return DXGI_FORMAT_R8G8B8A8_UNORM;

	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		return DXGI_FORMAT_B8G8R8A8_UNORM;

	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		return DXGI_FORMAT_B8G8R8X8_UNORM;

	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_R32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;

	default:
		return format;
	}
}

DXGI_FORMAT Carol::GetDsvFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
		return DXGI_FORMAT_D32_FLOAT;

	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;

	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
		return DXGI_FORMAT_D16_UNORM;

	default:
		return format;
	}
}

uint32_t Carol::GetPlaneSize(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_NV12:
		return 2;
	default:
		return 1;
	}
}

Carol::Resource::Resource()
	:mAllocInfo(make_unique<HeapAllocInfo>())
{
}

Carol::Resource::Resource(D3D12_RESOURCE_DESC* desc, Heap* heap, D3D12_RESOURCE_STATES initState, D3D12_CLEAR_VALUE* optimizedClearValue)
	:mAllocInfo(make_unique<HeapAllocInfo>()), mIntermediateBufferAllocInfo(make_unique<HeapAllocInfo>())
{
	heap->CreateResource(&mResource, desc, mAllocInfo.get(), initState, optimizedClearValue);
}

Carol::Resource::~Resource()
{
	if (mMappedData)
	{
		mResource->Unmap(0, nullptr);
		mMappedData = nullptr;
	}

	if (mAllocInfo->Heap)
	{
		mAllocInfo->Heap->DeleteResource(mAllocInfo.get());
	}
}

ID3D12Resource* Carol::Resource::Get()
{
	return mResource.Get();
}

ID3D12Resource** Carol::Resource::GetAddressOf()
{
	return mResource.GetAddressOf();
}

D3D12_GPU_VIRTUAL_ADDRESS Carol::Resource::GetGPUVirtualAddress()
{
	return mResource->GetGPUVirtualAddress();
}

void Carol::Resource::CopySubresources(ID3D12GraphicsCommandList* cmdList, Heap* intermediateHeap, const void* data, uint32_t byteSize)
{
	D3D12_SUBRESOURCE_DATA subresource;
	subresource.RowPitch = byteSize;
	subresource.SlicePitch = subresource.RowPitch;
	subresource.pData = data;

	cmdList->ResourceBarrier(1,GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mResource.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST)));
	auto resourceSize = GetRequiredIntermediateSize(mResource.Get(), 0, 1);
	intermediateHeap->CreateResource(&mIntermediateBuffer, GetRvaluePtr(CD3DX12_RESOURCE_DESC::Buffer(resourceSize)), mIntermediateBufferAllocInfo.get(), D3D12_RESOURCE_STATE_GENERIC_READ);
	
	UpdateSubresources(cmdList, mResource.Get(), mIntermediateBuffer.Get(), 0, 0, 1, &subresource);
	cmdList->ResourceBarrier(1,GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ)));
}

void Carol::Resource::CopySubresources(ID3D12GraphicsCommandList* cmdList, Heap* intermediateHeap, D3D12_SUBRESOURCE_DATA* subresources, uint32_t firstSubresource, uint32_t numSubresources)
{
	cmdList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mResource.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST)));
	auto resourceSize = GetRequiredIntermediateSize(mResource.Get(), firstSubresource, numSubresources);
	intermediateHeap->CreateResource(&mIntermediateBuffer, GetRvaluePtr(CD3DX12_RESOURCE_DESC::Buffer(resourceSize)), mIntermediateBufferAllocInfo.get(), D3D12_RESOURCE_STATE_GENERIC_READ);

	UpdateSubresources(cmdList, mResource.Get(), mIntermediateBuffer.Get(), 0, firstSubresource, numSubresources, subresources);
	cmdList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ)));
}

void Carol::Resource::CopyData(const void* data, uint32_t byteSize)
{
	if (!mMappedData)
	{
		ThrowIfFailed(mResource->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));
	}

	memcpy(mMappedData, data, byteSize);
}

void Carol::Resource::ReleaseIntermediateBuffer()
{
	if (mIntermediateBuffer)
	{
		mIntermediateBufferAllocInfo->Heap->DeleteResourceImmediate(mIntermediateBufferAllocInfo.get());
		mIntermediateBuffer.Reset();
	}
}

Carol::Buffer::Buffer(DescriptorAllocator* cbvSrvUavAllocator, DescriptorAllocator* rtvAllocator, DescriptorAllocator* dsvAllocator)
	:mCbvSrvUavAllocator(cbvSrvUavAllocator),
	mRtvAllocator(rtvAllocator),
	mDsvAllocator(dsvAllocator),
	mCpuSrvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mGpuSrvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mCpuUavAllocInfo(make_unique<DescriptorAllocInfo>()),
	mGpuUavAllocInfo(make_unique<DescriptorAllocInfo>()),
	mRtvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mDsvAllocInfo(make_unique<DescriptorAllocInfo>())
{
}

Carol::Buffer::~Buffer()
{
	if (mCpuSrvAllocInfo->Allocator)
	{
		mCpuSrvAllocInfo->Allocator->CpuDeallocate(mCpuSrvAllocInfo.get());
	}

	if (mGpuSrvAllocInfo->Allocator)
	{
		mGpuSrvAllocInfo->Allocator->GpuDeallocate(mGpuSrvAllocInfo.get());
	}

	if (mCpuUavAllocInfo->Allocator)
	{
		mCpuUavAllocInfo->Allocator->CpuDeallocate(mCpuUavAllocInfo.get());
	}

	if (mGpuUavAllocInfo->Allocator)
	{
		mGpuUavAllocInfo->Allocator->GpuDeallocate(mGpuUavAllocInfo.get());
	}

	if (mRtvAllocInfo->Allocator)
	{
		mRtvAllocInfo->Allocator->CpuDeallocate(mRtvAllocInfo.get());
	}
	
	if (mDsvAllocInfo->Allocator)
	{
		mDsvAllocInfo->Allocator->CpuDeallocate(mDsvAllocInfo.get());
	}
}

ID3D12Resource* Carol::Buffer::Get()
{
	return mResource->Get();
}

ID3D12Resource** Carol::Buffer::GetAddressOf()
{
	return mResource->GetAddressOf();
}

D3D12_GPU_VIRTUAL_ADDRESS Carol::Buffer::GetGPUVirtualAddress()
{
	return mResource->GetGPUVirtualAddress();
}

void Carol::Buffer::CopySubresources(ID3D12GraphicsCommandList* cmdList, Heap* intermediateHeap, const void* data, uint32_t byteSize)
{
	mResource->CopySubresources(cmdList, intermediateHeap, data, byteSize);
}

void Carol::Buffer::CopySubresources(ID3D12GraphicsCommandList* cmdList, Heap* intermediateHeap, D3D12_SUBRESOURCE_DATA* subresources, uint32_t firstSubresource, uint32_t numSubresources)
{
	mResource->CopySubresources(cmdList, intermediateHeap, subresources, firstSubresource, numSubresources);
}

void Carol::Buffer::CopyData(const void* data, uint32_t byteSize)
{
	mResource->CopyData(data, byteSize);
}

void Carol::Buffer::ReleaseIntermediateBuffer()
{
	mResource->ReleaseIntermediateBuffer();
}

D3D12_CPU_DESCRIPTOR_HANDLE Carol::Buffer::GetCpuSrv(uint32_t planeSlice)
{
	return mCbvSrvUavAllocator->GetCpuHandle(mCpuSrvAllocInfo.get(), planeSlice);
}

D3D12_CPU_DESCRIPTOR_HANDLE Carol::Buffer::GetShaderCpuSrv(uint32_t planeSlice)
{
	return mCbvSrvUavAllocator->GetShaderCpuHandle(mGpuSrvAllocInfo.get(), planeSlice);
}

D3D12_GPU_DESCRIPTOR_HANDLE Carol::Buffer::GetGpuSrv(uint32_t planeSlice)
{
	return mCbvSrvUavAllocator->GetGpuHandle(mGpuSrvAllocInfo.get(), planeSlice);
}

uint32_t Carol::Buffer::GetGpuSrvIdx(uint32_t planeSlice)
{
	return mGpuSrvAllocInfo->StartOffset + planeSlice;
}

D3D12_CPU_DESCRIPTOR_HANDLE Carol::Buffer::GetCpuUav(uint32_t mipSlice, uint32_t planeSlice)
{
	return mCbvSrvUavAllocator->GetCpuHandle(mCpuUavAllocInfo.get(), mipSlice + planeSlice * mResourceDesc.MipLevels);
}

D3D12_CPU_DESCRIPTOR_HANDLE Carol::Buffer::GetShaderCpuUav(uint32_t mipSlice, uint32_t planeSlice)
{
	return mCbvSrvUavAllocator->GetCpuHandle(mGpuUavAllocInfo.get(), mipSlice + planeSlice * mResourceDesc.MipLevels);
}

D3D12_GPU_DESCRIPTOR_HANDLE Carol::Buffer::GetGpuUav(uint32_t mipSlice, uint32_t planeSlice)
{
	return mCbvSrvUavAllocator->GetGpuHandle(mGpuUavAllocInfo.get(), mipSlice + planeSlice * mResourceDesc.MipLevels);
}

uint32_t Carol::Buffer::GetGpuUavIdx(uint32_t mipSlice, uint32_t planeSlice)
{
	return mGpuUavAllocInfo->StartOffset + mipSlice + planeSlice * mResourceDesc.MipLevels;
}

D3D12_CPU_DESCRIPTOR_HANDLE Carol::Buffer::GetRtv(uint32_t mipSlice, uint32_t planeSlice)
{
	return mRtvAllocator->GetCpuHandle(mRtvAllocInfo.get(), mipSlice + planeSlice * mResourceDesc.MipLevels);
}

D3D12_CPU_DESCRIPTOR_HANDLE Carol::Buffer::GetDsv(uint32_t mipSlice)
{
	return mDsvAllocator->GetCpuHandle(mDsvAllocInfo.get(), mipSlice);
}



void Carol::Buffer::BindDescriptors()
{
	BindSrv();

	if (mResourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
	{
		BindUav();
	}

	if (mResourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
	{
		BindRtv();
	}

	if (mResourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	{
		BindDsv();
	}

	CopyDescriptors();
}

void Carol::Buffer::CopyDescriptors()
{
	mCbvSrvUavAllocator->CopyDescriptors(mCpuSrvAllocInfo.get(), mGpuSrvAllocInfo.get());
	mCbvSrvUavAllocator->CopyDescriptors(mCpuUavAllocInfo.get(), mGpuUavAllocInfo.get());
}

Carol::ColorBuffer::ColorBuffer(
	uint32_t width,
	uint32_t height,
	uint32_t depthOrArraySize,
	ColorBufferViewDimension viewDimension,
	DXGI_FORMAT format,
	Heap* heap,
	D3D12_RESOURCE_STATES initState,
	DescriptorAllocator* cbvSrvUavAllocator,
	DescriptorAllocator* rtvAllocator,
	DescriptorAllocator* dsvAllocator,
	D3D12_RESOURCE_FLAGS flags,
	D3D12_CLEAR_VALUE* optClearValue,
	uint32_t mipLevels,
	uint32_t viewMipLevels,
	uint32_t mostDetailedMip,
	float resourceMinLODClamp,
	uint32_t viewArraySize,
	uint32_t firstArraySlice,
	uint32_t sampleCount,
	uint32_t sampleQuality)
	:Buffer(cbvSrvUavAllocator, rtvAllocator, dsvAllocator),
	mViewDimension(viewDimension),
	mFormat(format),
	mViewMipLevels(viewMipLevels == 0 ? mipLevels : viewMipLevels),
	mMostDetailedMip(mostDetailedMip),
	mResourceMinLODClamp(resourceMinLODClamp),
	mViewArraySize(viewArraySize == 0 ? depthOrArraySize : viewArraySize),
	mFirstArraySlice(firstArraySlice)
{
	mPlaneSize = GetPlaneSize(mFormat);

	mResourceDesc.Dimension = GetResourceDimension(mViewDimension);
	mResourceDesc.Format = GetBaseFormat(mFormat);
	mResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	mResourceDesc.Flags = flags;
	mResourceDesc.SampleDesc.Count = sampleCount;
	mResourceDesc.SampleDesc.Quality = sampleQuality;
	mResourceDesc.Width = width;
	mResourceDesc.Height = height;
	mResourceDesc.DepthOrArraySize = depthOrArraySize;
	mResourceDesc.MipLevels = mipLevels;
	mResourceDesc.Alignment = 0Ui64;
	
	mResource = make_unique<Resource>(&mResourceDesc, heap, initState, optClearValue);
	BindDescriptors();
}

void Carol::ColorBuffer::BindSrv()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = mFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	
	vector<D3D12_SHADER_RESOURCE_VIEW_DESC> srvDescs;

	switch (mViewDimension)
	{
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE1D:
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
		srvDesc.Texture1D.MipLevels = mViewMipLevels;
		srvDesc.Texture1D.MostDetailedMip = mMostDetailedMip;
		srvDesc.Texture1D.ResourceMinLODClamp = mResourceMinLODClamp;

		srvDescs.push_back(srvDesc);
		break;
	}
		
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE1DARRAY:
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
		srvDesc.Texture1DArray.ArraySize = mViewArraySize;
		srvDesc.Texture1DArray.FirstArraySlice = mFirstArraySlice;
		srvDesc.Texture1DArray.MipLevels = mViewMipLevels;
		srvDesc.Texture1DArray.MostDetailedMip = mMostDetailedMip;
		srvDesc.Texture1DArray.ResourceMinLODClamp = mResourceMinLODClamp;

		srvDescs.push_back(srvDesc);
		break;
	}

	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D:
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = mViewMipLevels;
		srvDesc.Texture2D.MostDetailedMip = mMostDetailedMip;
		srvDesc.Texture2D.ResourceMinLODClamp = mResourceMinLODClamp;

		for (int i = 0; i < mPlaneSize; ++i)
		{
			srvDesc.Texture2D.PlaneSlice = i;
			srvDescs.push_back(srvDesc);
		}

		break;
	}
		
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DARRAY:
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
		srvDesc.Texture2DArray.ArraySize = mViewArraySize;
		srvDesc.Texture2DArray.FirstArraySlice = mFirstArraySlice;
		srvDesc.Texture2DArray.MipLevels = mViewMipLevels;
		srvDesc.Texture2DArray.MostDetailedMip = mMostDetailedMip;
		srvDesc.Texture2DArray.ResourceMinLODClamp = mResourceMinLODClamp;

		for (int i = 0; i < mPlaneSize; ++i)
		{
			srvDesc.Texture2DArray.PlaneSlice = i;
			srvDescs.push_back(srvDesc);
		}

		break;
	}

	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DMS:
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;

		srvDescs.push_back(srvDesc);
		break;
	}
	
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DMSARRAY:
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
		srvDesc.Texture2DMSArray.ArraySize = mViewArraySize;
		srvDesc.Texture2DMSArray.FirstArraySlice = mFirstArraySlice;

		srvDescs.push_back(srvDesc);
		break;
	}
	
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE3D:
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MipLevels = mViewMipLevels;
		srvDesc.Texture3D.MostDetailedMip = mMostDetailedMip;
		srvDesc.Texture3D.ResourceMinLODClamp = mResourceMinLODClamp;

		srvDescs.push_back(srvDesc);
		break;
	}
	
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURECUBE:
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = mViewMipLevels;
		srvDesc.TextureCube.MostDetailedMip = mMostDetailedMip;
		srvDesc.TextureCube.ResourceMinLODClamp = mResourceMinLODClamp;

		srvDescs.push_back(srvDesc);
		break;
	}
	
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURECUBEARRAY:
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
		srvDesc.TextureCubeArray.NumCubes = mViewArraySize;
		srvDesc.TextureCubeArray.First2DArrayFace = mFirstArraySlice;
		srvDesc.TextureCubeArray.MipLevels = mViewMipLevels;
		srvDesc.TextureCubeArray.MostDetailedMip = mMostDetailedMip;
		srvDesc.TextureCubeArray.ResourceMinLODClamp = mResourceMinLODClamp;

		srvDescs.push_back(srvDesc);
		break;
	}

	default:
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_UNKNOWN;

		srvDescs.push_back(srvDesc);
		break;
	};

	mCbvSrvUavAllocator->CpuAllocate(srvDescs.size(), mCpuSrvAllocInfo.get());
	mCbvSrvUavAllocator->GpuAllocate(srvDescs.size(), mGpuSrvAllocInfo.get());

	for (int i = 0; i < srvDescs.size(); ++i)
	{
		mCbvSrvUavAllocator->CreateSrv(&srvDescs[i], mResource.get(), mCpuSrvAllocInfo.get(), i);
	}
}

void Carol::ColorBuffer::BindUav()
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = GetUavFormat(mFormat);
	
	vector<D3D12_UNORDERED_ACCESS_VIEW_DESC> uavDescs;

	switch (mViewDimension)
	{
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE1D:
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
		
		for (int i = 0; i < mResourceDesc.MipLevels; ++i)
		{
			uavDesc.Texture1D.MipSlice = i;
			uavDescs.push_back(uavDesc);
		}

		break;
	}
		
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE1DARRAY:
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
		uavDesc.Texture1DArray.ArraySize = mViewArraySize;
		uavDesc.Texture1DArray.FirstArraySlice = mFirstArraySlice;
		
		for (int i = 0; i < mResourceDesc.MipLevels; ++i)
		{
			uavDesc.Texture1DArray.MipSlice = i;
			uavDescs.push_back(uavDesc);
		}

		break;
	}

	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D:
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		for (int i = 0; i < mPlaneSize; ++i)
		{
			for (int j = 0; j < mResourceDesc.MipLevels; ++j)
			{
				uavDesc.Texture2D.PlaneSlice = i;
				uavDesc.Texture2D.MipSlice = j;
				uavDescs.push_back(uavDesc);
			}
		}

		break;
	}
		
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DARRAY:
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
		uavDesc.Texture2DArray.ArraySize = mViewArraySize;
		uavDesc.Texture2DArray.FirstArraySlice = mFirstArraySlice;

		for (int i = 0; i < mPlaneSize; ++i)
		{
			for (int j = 0; j < mResourceDesc.MipLevels; ++j)
			{
				uavDesc.Texture2DArray.PlaneSlice = i;
				uavDesc.Texture2DArray.MipSlice = j;
				uavDescs.push_back(uavDesc);
			}
		}

		break;
	}

	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DMS:
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DMS;

		uavDescs.push_back(uavDesc);
		break;
	}
	
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DMSARRAY:
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DMSARRAY;
		uavDesc.Texture2DMSArray.ArraySize = mViewArraySize;
		uavDesc.Texture2DMSArray.FirstArraySlice = mFirstArraySlice;

		uavDescs.push_back(uavDesc);
		break;
	}
	
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE3D:
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		uavDesc.Texture3D.WSize = mViewArraySize;
		uavDesc.Texture3D.FirstWSlice = mFirstArraySlice;
		
		for (int i = 0; i < mResourceDesc.MipLevels; ++i)
		{
			uavDesc.Texture3D.MipSlice = i;
			uavDescs.push_back(uavDesc);
		}

		break;
	}

	default:
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_UNKNOWN;

		uavDescs.push_back(uavDesc);
		break;
	};

	mCbvSrvUavAllocator->CpuAllocate(uavDescs.size(), mCpuUavAllocInfo.get());
	mCbvSrvUavAllocator->GpuAllocate(uavDescs.size(), mGpuUavAllocInfo.get());

	for (int i = 0; i < uavDescs.size(); ++i)
	{
		mCbvSrvUavAllocator->CreateUav(&uavDescs[i], mResource.get(), nullptr, mCpuUavAllocInfo.get(), i);
	}
}

void Carol::ColorBuffer::BindRtv()
{
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = mFormat;

	vector<D3D12_RENDER_TARGET_VIEW_DESC> rtvDescs;

	switch (mViewDimension)
	{
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE1D:
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;

		for (int i = 0; i < mResourceDesc.MipLevels; ++i)
		{
			rtvDesc.Texture1D.MipSlice = i;
			rtvDescs.push_back(rtvDesc);
		}

		break;
	}

	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE1DARRAY:
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
		rtvDesc.Texture1DArray.ArraySize = mViewArraySize;
		rtvDesc.Texture1DArray.FirstArraySlice = mFirstArraySlice;

		for (int i = 0; i < mResourceDesc.MipLevels; ++i)
		{
			rtvDesc.Texture1DArray.MipSlice = i;
			rtvDescs.push_back(rtvDesc);
		}

		break;
	}

	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D:
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		for (int i = 0; i < mPlaneSize; ++i)
		{
			for (int j = 0; j < mResourceDesc.MipLevels; ++j)
			{
				rtvDesc.Texture2D.PlaneSlice = i;
				rtvDesc.Texture2D.MipSlice = j;
				rtvDescs.push_back(rtvDesc);
			}
		}

		break;
	}

	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DARRAY:
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
		rtvDesc.Texture2DArray.ArraySize = mViewArraySize;
		rtvDesc.Texture2DArray.FirstArraySlice = mFirstArraySlice;

		for (int i = 0; i < mPlaneSize; ++i)
		{
			for (int j = 0; j < mResourceDesc.MipLevels; ++j)
			{
				rtvDesc.Texture2DArray.PlaneSlice = i;
				rtvDesc.Texture2DArray.MipSlice = j;
				rtvDescs.push_back(rtvDesc);
			}
		}

		break;
	}

	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DMS:
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;

		rtvDescs.push_back(rtvDesc);
		break;
	}

	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DMSARRAY:
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
		rtvDesc.Texture2DMSArray.ArraySize = mViewArraySize;
		rtvDesc.Texture2DMSArray.FirstArraySlice = mFirstArraySlice;

		rtvDescs.push_back(rtvDesc);
		break;
	}

	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE3D:
	{
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
		rtvDesc.Texture3D.WSize = mViewArraySize;
		rtvDesc.Texture3D.FirstWSlice = mFirstArraySlice;

		for (int i = 0; i < mResourceDesc.MipLevels; ++i)
		{
			rtvDesc.Texture3D.MipSlice = i;
			rtvDescs.push_back(rtvDesc);
		}
		break;
	}

	default:
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_UNKNOWN;

		rtvDescs.push_back(rtvDesc);
		break;
	}

	mRtvAllocator->CpuAllocate(rtvDescs.size(), mRtvAllocInfo.get());

	for (int i = 0; i < rtvDescs.size(); ++i)
	{
		mRtvAllocator->CreateRtv(&rtvDescs[i], mResource.get(), mRtvAllocInfo.get(), i);
	}
}

void Carol::ColorBuffer::BindDsv()
{
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Format = GetDsvFormat(mFormat);
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	
	vector<D3D12_DEPTH_STENCIL_VIEW_DESC> dsvDescs;

	switch (mViewDimension)
	{
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE1D:
	{
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
		
		for (int i = 0; i < mResourceDesc.MipLevels; ++i)
		{
			dsvDesc.Texture1D.MipSlice = i;
			dsvDescs.push_back(dsvDesc);
		}
		break;
	}
		
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE1DARRAY:
	{
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
		dsvDesc.Texture1DArray.ArraySize = mViewArraySize;
		dsvDesc.Texture1DArray.FirstArraySlice = mFirstArraySlice;
		
		for (int i = 0; i < mResourceDesc.MipLevels; ++i)
		{
			dsvDesc.Texture1DArray.MipSlice = i;
			dsvDescs.push_back(dsvDesc);
		}

		break;
	}

	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D:
	{
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

		for (int i = 0; i < mResourceDesc.MipLevels; ++i)
		{
			dsvDesc.Texture2D.MipSlice = i;
			dsvDescs.push_back(dsvDesc);
		}

		break;
	}
		
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DARRAY:
	{
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
		dsvDesc.Texture2DArray.ArraySize = mViewArraySize;
		dsvDesc.Texture2DArray.FirstArraySlice = mFirstArraySlice;

		for (int i = 0; i < mResourceDesc.MipLevels; ++i)
		{
			dsvDesc.Texture2DArray.MipSlice = i;
			dsvDescs.push_back(dsvDesc);
		}

		break;
	}

	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DMS:
	{
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
		
		dsvDescs.push_back(dsvDesc);
		break;
	}
	
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DMSARRAY:
	{
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
		dsvDesc.Texture2DMSArray.ArraySize = mViewArraySize;
		dsvDesc.Texture2DMSArray.FirstArraySlice = mFirstArraySlice;

		dsvDescs.push_back(dsvDesc);
		break;
	}

	default:
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_UNKNOWN;

		dsvDescs.push_back(dsvDesc);
		break;
	};

	mDsvAllocator->CpuAllocate(dsvDescs.size(), mDsvAllocInfo.get());

	for (int i = 0; i < dsvDescs.size(); ++i)
	{
		mDsvAllocator->CreateDsv(&dsvDescs[i], mResource.get(), mDsvAllocInfo.get(), i);
	}

}

D3D12_RESOURCE_DIMENSION Carol::ColorBuffer::GetResourceDimension(ColorBufferViewDimension viewDimension)
{
	switch(viewDimension)
	{
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE1D:
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE1DARRAY:
		return D3D12_RESOURCE_DIMENSION_TEXTURE1D;

	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D:
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DARRAY:
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DMS:
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DMSARRAY:
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURECUBE:
	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURECUBEARRAY:
		return D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	case COLOR_BUFFER_VIEW_DIMENSION_TEXTURE3D:
		return D3D12_RESOURCE_DIMENSION_TEXTURE3D;

	default:
		return D3D12_RESOURCE_DIMENSION_UNKNOWN;
	}
}

Carol::StructuredBuffer::StructuredBuffer(
	uint32_t numElements,
	uint32_t elementSize,
	Heap* heap,
	D3D12_RESOURCE_STATES initState,
	DescriptorAllocator* cbvSrvUavAllocator,
	D3D12_RESOURCE_FLAGS flags,
	uint32_t viewNumElements,
	uint32_t firstElement)
	:Buffer(cbvSrvUavAllocator, nullptr, nullptr),
	mNumElements(numElements),
	mElementSize(elementSize),
	mViewNumElements(viewNumElements == 0 ? numElements : viewNumElements),
	mFirstElement(firstElement)
{
	mResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	mResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	mResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	mResourceDesc.Flags = flags;
	mResourceDesc.SampleDesc.Count = 1u;
	mResourceDesc.SampleDesc.Quality = 0u;
	mResourceDesc.Width = (flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) ? mNumElements * mElementSize + 4096 : mNumElements * mElementSize;
	mResourceDesc.Height = 1u;
	mResourceDesc.DepthOrArraySize = 1ui16;
	mResourceDesc.MipLevels = 1ui16;
	mResourceDesc.Alignment = 0ui64;

	mResource = make_unique<Resource>(&mResourceDesc, heap, initState);
	BindDescriptors();
}

void Carol::StructuredBuffer::BindSrv()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.NumElements = mViewNumElements;
	srvDesc.Buffer.FirstElement = mFirstElement;
	srvDesc.Buffer.StructureByteStride = mElementSize;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	mCbvSrvUavAllocator->CpuAllocate(1, mCpuSrvAllocInfo.get());
	mCbvSrvUavAllocator->GpuAllocate(1, mGpuSrvAllocInfo.get());
	mCbvSrvUavAllocator->CreateSrv(&srvDesc, mResource.get(), mCpuSrvAllocInfo.get());
}

void Carol::StructuredBuffer::BindUav()
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.Buffer.NumElements = mViewNumElements;
	uavDesc.Buffer.FirstElement = mFirstElement;
	uavDesc.Buffer.StructureByteStride = mElementSize;
	uavDesc.Buffer.CounterOffsetInBytes = floorf(mResourceDesc.Width / 4096.f) * 4096;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	mCbvSrvUavAllocator->CpuAllocate(1, mCpuUavAllocInfo.get());
	mCbvSrvUavAllocator->GpuAllocate(1, mGpuUavAllocInfo.get());
	mCbvSrvUavAllocator->CreateUav(&uavDesc, mResource.get(), mResource.get(), mCpuUavAllocInfo.get());
}

void Carol::StructuredBuffer::BindRtv()
{
}

void Carol::StructuredBuffer::BindDsv()
{
}

Carol::RawBuffer::RawBuffer(
	uint32_t byteSize,
	Heap* heap,
	D3D12_RESOURCE_STATES initState,
	DescriptorAllocator* cbvSrvUavAllocator,
	D3D12_RESOURCE_FLAGS flags)
	:Buffer(cbvSrvUavAllocator, nullptr, nullptr)
{
	mResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	mResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	mResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	mResourceDesc.Flags = flags;
	mResourceDesc.SampleDesc.Count = 1u;
	mResourceDesc.SampleDesc.Quality = 0u;
	mResourceDesc.Width = byteSize;
	mResourceDesc.Height = 1u;
	mResourceDesc.DepthOrArraySize = 1ui16;
	mResourceDesc.MipLevels = 1ui16;
	mResourceDesc.Alignment = 0ui64;

	mResource = make_unique<Resource>(&mResourceDesc, heap, initState);
	BindDescriptors();
}

void Carol::RawBuffer::BindSrv()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.NumElements = 1;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.StructureByteStride = 0;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

	mCbvSrvUavAllocator->CpuAllocate(1, mCpuSrvAllocInfo.get());
	mCbvSrvUavAllocator->GpuAllocate(1, mGpuSrvAllocInfo.get());
	mCbvSrvUavAllocator->CreateSrv(&srvDesc, mResource.get(), mCpuSrvAllocInfo.get());
}

void Carol::RawBuffer::BindUav()
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	uavDesc.Buffer.NumElements = 1;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.StructureByteStride = 0;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

	mCbvSrvUavAllocator->CpuAllocate(1, mCpuUavAllocInfo.get());
	mCbvSrvUavAllocator->GpuAllocate(1, mGpuUavAllocInfo.get());
	mCbvSrvUavAllocator->CreateUav(&uavDesc, mResource.get(), nullptr, mCpuUavAllocInfo.get());
}

void Carol::RawBuffer::BindRtv()
{

}

void Carol::RawBuffer::BindDsv()
{

}





