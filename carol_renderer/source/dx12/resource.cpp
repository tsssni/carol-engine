#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/descriptor.h>
#include <global.h>
#include <vector>

namespace Carol {
	using std::unique_ptr;
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
	if (mAllocInfo->Heap)
	{
		mMappedData = nullptr;
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

byte* Carol::Resource::GetMappedData()
{
	return mMappedData;
}

Carol::Heap* Carol::Resource::GetHeap()
{
	return mAllocInfo->Heap;
}

D3D12_GPU_VIRTUAL_ADDRESS Carol::Resource::GetGPUVirtualAddress()
{
	return mResource->GetGPUVirtualAddress();
}

void Carol::Resource::CopySubresources(Heap* intermediateHeap, const void* data, uint32_t byteSize, D3D12_RESOURCE_STATES beforeState)
{
	D3D12_SUBRESOURCE_DATA subresource;
	subresource.RowPitch = byteSize;
	subresource.SlicePitch = subresource.RowPitch;
	subresource.pData = data;

	gCommandList->ResourceBarrier(1,GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mResource.Get(), beforeState, D3D12_RESOURCE_STATE_COPY_DEST)));
	auto resourceSize = GetRequiredIntermediateSize(mResource.Get(), 0, 1);
	intermediateHeap->CreateResource(&mIntermediateBuffer, GetRvaluePtr(CD3DX12_RESOURCE_DESC::Buffer(resourceSize)), mIntermediateBufferAllocInfo.get(), D3D12_RESOURCE_STATE_COMMON);
	
	UpdateSubresources(gCommandList.Get(), mResource.Get(), mIntermediateBuffer.Get(), 0, 0, 1, &subresource);
	gCommandList->ResourceBarrier(1,GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, beforeState)));
}

void Carol::Resource::CopySubresources(Heap* intermediateHeap, D3D12_SUBRESOURCE_DATA* subresources, uint32_t firstSubresource, uint32_t numSubresources, D3D12_RESOURCE_STATES beforeState)
{
	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mResource.Get(), beforeState, D3D12_RESOURCE_STATE_COPY_DEST)));
	auto resourceSize = GetRequiredIntermediateSize(mResource.Get(), firstSubresource, numSubresources);
	intermediateHeap->CreateResource(&mIntermediateBuffer, GetRvaluePtr(CD3DX12_RESOURCE_DESC::Buffer(resourceSize)), mIntermediateBufferAllocInfo.get(), D3D12_RESOURCE_STATE_COMMON);

	UpdateSubresources(gCommandList.Get(), mResource.Get(), mIntermediateBuffer.Get(), 0, firstSubresource, numSubresources, subresources);
	gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, beforeState)));
}
#include <scene/model.h>
void Carol::Resource::CopyData(const void* data, uint32_t byteSize, uint32_t offset)
{
	if (!mMappedData)
	{
		ThrowIfFailed(mResource->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));
		mAllocInfo->MappedData = mMappedData;
	}

	memcpy(mMappedData + offset, data, byteSize);
	MeshConstants* mc = reinterpret_cast<MeshConstants*>(mMappedData + offset);
}

void Carol::Resource::ReleaseIntermediateBuffer()
{
	if (mIntermediateBuffer)
	{
		mIntermediateBufferAllocInfo->Heap->DeleteResourceImmediate(mIntermediateBufferAllocInfo.get());
		mIntermediateBuffer.Reset();
	}
}

Carol::Buffer::Buffer()
	:mCpuSrvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mGpuSrvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mCpuUavAllocInfo(make_unique<DescriptorAllocInfo>()),
	mGpuUavAllocInfo(make_unique<DescriptorAllocInfo>()),
	mRtvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mDsvAllocInfo(make_unique<DescriptorAllocInfo>())
{
}

Carol::Buffer::Buffer(Buffer&& buffer)
{
	mResource = std::move(buffer.mResource);
	mResourceDesc = buffer.mResourceDesc;
	
	mCpuSrvAllocInfo = std::move(buffer.mCpuSrvAllocInfo);
	mGpuSrvAllocInfo = std::move(buffer.mGpuSrvAllocInfo);
		
	mCpuUavAllocInfo = std::move(buffer.mCpuUavAllocInfo);
	mGpuUavAllocInfo = std::move(buffer.mGpuUavAllocInfo);

	mRtvAllocInfo = std::move(buffer.mRtvAllocInfo);
	mDsvAllocInfo = std::move(buffer.mDsvAllocInfo);
}

Carol::Buffer& Carol::Buffer::operator=(Buffer&& buffer)
{
	this->Buffer::Buffer(std::move(buffer));
	return *this;
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

void Carol::Buffer::CopySubresources(Heap* intermediateHeap, const void* data, uint32_t byteSize, D3D12_RESOURCE_STATES beforeState)
{
	mResource->CopySubresources(intermediateHeap, data, byteSize, beforeState);
}

void Carol::Buffer::CopySubresources(Heap* intermediateHeap, D3D12_SUBRESOURCE_DATA* subresources, uint32_t firstSubresource, uint32_t numSubresources, D3D12_RESOURCE_STATES beforeState)
{
	mResource->CopySubresources(intermediateHeap, subresources, firstSubresource, numSubresources, beforeState);
}

void Carol::Buffer::CopyData(const void* data, uint32_t byteSize, uint32_t offset)
{
	mResource->CopyData(data, byteSize, offset);
}

void Carol::Buffer::ReleaseIntermediateBuffer()
{
	mResource->ReleaseIntermediateBuffer();
}

D3D12_CPU_DESCRIPTOR_HANDLE Carol::Buffer::GetCpuSrv(uint32_t planeSlice)
{
	return gDescriptorManager->GetCpuCbvSrvUavHandle(mCpuSrvAllocInfo.get(), planeSlice);
}

D3D12_CPU_DESCRIPTOR_HANDLE Carol::Buffer::GetShaderCpuSrv(uint32_t planeSlice)
{
	return gDescriptorManager->GetShaderCpuCbvSrvUavHandle(mGpuSrvAllocInfo.get(), planeSlice);
}

D3D12_GPU_DESCRIPTOR_HANDLE Carol::Buffer::GetGpuSrv(uint32_t planeSlice)
{
	return gDescriptorManager->GetGpuCbvSrvUavHandle(mGpuSrvAllocInfo.get(), planeSlice);
}

uint32_t Carol::Buffer::GetGpuSrvIdx(uint32_t planeSlice)
{
	return mGpuSrvAllocInfo->StartOffset + planeSlice;
}

D3D12_CPU_DESCRIPTOR_HANDLE Carol::Buffer::GetCpuUav(uint32_t mipSlice, uint32_t planeSlice)
{
	return gDescriptorManager->GetCpuCbvSrvUavHandle(mCpuUavAllocInfo.get(), mipSlice + planeSlice * mResourceDesc.MipLevels);
}

D3D12_CPU_DESCRIPTOR_HANDLE Carol::Buffer::GetShaderCpuUav(uint32_t mipSlice, uint32_t planeSlice)
{
	return gDescriptorManager->GetShaderCpuCbvSrvUavHandle(mGpuUavAllocInfo.get(), mipSlice + planeSlice * mResourceDesc.MipLevels);
}

D3D12_GPU_DESCRIPTOR_HANDLE Carol::Buffer::GetGpuUav(uint32_t mipSlice, uint32_t planeSlice)
{
	return gDescriptorManager->GetGpuCbvSrvUavHandle(mGpuUavAllocInfo.get(), mipSlice + planeSlice * mResourceDesc.MipLevels);
}

uint32_t Carol::Buffer::GetGpuUavIdx(uint32_t mipSlice, uint32_t planeSlice)
{
	return mGpuUavAllocInfo->StartOffset + mipSlice + planeSlice * mResourceDesc.MipLevels;
}

D3D12_CPU_DESCRIPTOR_HANDLE Carol::Buffer::GetRtv(uint32_t mipSlice, uint32_t planeSlice)
{
	return gDescriptorManager->GetRtvHandle(mRtvAllocInfo.get(), mipSlice + planeSlice * mResourceDesc.MipLevels);
}

D3D12_CPU_DESCRIPTOR_HANDLE Carol::Buffer::GetDsv(uint32_t mipSlice)
{
	return gDescriptorManager->GetDsvHandle(mDsvAllocInfo.get(), mipSlice);
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

	CopyCbvSrvUav();
}

void Carol::Buffer::CopyCbvSrvUav()
{
	gDescriptorManager->CopyCbvSrvUav(mCpuSrvAllocInfo.get(), mGpuSrvAllocInfo.get());
	gDescriptorManager->CopyCbvSrvUav(mCpuUavAllocInfo.get(), mGpuUavAllocInfo.get());
}

Carol::ColorBuffer::ColorBuffer(
	uint32_t width,
	uint32_t height,
	uint32_t depthOrArraySize,
	ColorBufferViewDimension viewDimension,
	DXGI_FORMAT format,
	Heap* heap,
	D3D12_RESOURCE_STATES initState,
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
	:mViewDimension(viewDimension),
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

Carol::ColorBuffer::ColorBuffer(ColorBuffer&& colorBuffer)
	:Buffer(std::move(colorBuffer))
{
	mViewDimension=colorBuffer.mViewDimension;
	mFormat = colorBuffer.mFormat;

	mViewMipLevels = colorBuffer.mViewMipLevels;
	mMostDetailedMip = colorBuffer.mMostDetailedMip;
	mResourceMinLODClamp = colorBuffer.mResourceMinLODClamp;

	mViewArraySize = colorBuffer.mViewArraySize;
	mFirstArraySlice = colorBuffer.mFirstArraySlice;
	mPlaneSize = colorBuffer.mPlaneSize;
}

Carol::ColorBuffer& Carol::ColorBuffer::operator=(ColorBuffer&& colorBuffer)
{
	this->ColorBuffer::ColorBuffer(std::move(colorBuffer));
	return *this;
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

	gDescriptorManager->CpuCbvSrvUavAllocate(srvDescs.size(), mCpuSrvAllocInfo.get());
	gDescriptorManager->GpuCbvSrvUavAllocate(srvDescs.size(), mGpuSrvAllocInfo.get());

	for (int i = 0; i < srvDescs.size(); ++i)
	{
		gDescriptorManager->CreateSrv(&srvDescs[i], mResource.get(), mCpuSrvAllocInfo.get(), i);
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

	gDescriptorManager->CpuCbvSrvUavAllocate(uavDescs.size(), mCpuUavAllocInfo.get());
	gDescriptorManager->GpuCbvSrvUavAllocate(uavDescs.size(), mGpuUavAllocInfo.get());

	for (int i = 0; i < uavDescs.size(); ++i)
	{
		gDescriptorManager->CreateUav(&uavDescs[i], mResource.get(), nullptr, mCpuUavAllocInfo.get(), i);
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

	gDescriptorManager->RtvAllocate(rtvDescs.size(), mRtvAllocInfo.get());

	for (int i = 0; i < rtvDescs.size(); ++i)
	{
		gDescriptorManager->CreateRtv(&rtvDescs[i], mResource.get(), mRtvAllocInfo.get(), i);
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

	gDescriptorManager->DsvAllocate(dsvDescs.size(), mDsvAllocInfo.get());

	for (int i = 0; i < dsvDescs.size(); ++i)
	{
		gDescriptorManager->CreateDsv(&dsvDescs[i], mResource.get(), mDsvAllocInfo.get(), i);
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

Carol::unique_ptr<Carol::Resource> Carol::StructuredBuffer::sCounterResetBuffer = nullptr;

Carol::StructuredBuffer::StructuredBuffer(
	uint32_t numElements,
	uint32_t elementSize,
	Heap* heap,
	D3D12_RESOURCE_STATES initState,
	D3D12_RESOURCE_FLAGS flags,
	bool isConstant,
	uint32_t viewNumElements,
	uint32_t firstElement)
	:mNumElements(numElements),
	mElementSize(isConstant ? AlignForConstantBuffer(elementSize) : elementSize),
	mIsConstant(isConstant),
	mViewNumElements(viewNumElements == 0 ? numElements : viewNumElements),
	mFirstElement(firstElement)
{
	uint32_t byteSize = mNumElements * mElementSize;
	mCounterOffset = AlignForUavCounter(byteSize);

	mResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	mResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	mResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	mResourceDesc.Flags = flags;
	mResourceDesc.SampleDesc.Count = 1u;
	mResourceDesc.SampleDesc.Quality = 0u;
	mResourceDesc.Width = (flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) ? mCounterOffset + sizeof(uint32_t) : byteSize;
	mResourceDesc.Height = 1u;
	mResourceDesc.DepthOrArraySize = 1ui16;
	mResourceDesc.MipLevels = 1ui16;
	mResourceDesc.Alignment = 0ui64;

	mResource = make_unique<Resource>(&mResourceDesc, heap, initState);
	BindDescriptors();
}

Carol::StructuredBuffer::StructuredBuffer(StructuredBuffer&& structuredBuffer)
	:Buffer(std::move(structuredBuffer))
{
	mNumElements = structuredBuffer.mNumElements;
	mElementSize = structuredBuffer.mElementSize;
	mViewNumElements = structuredBuffer.mViewNumElements;
	mFirstElement = structuredBuffer.mFirstElement;
}

Carol::StructuredBuffer& Carol::StructuredBuffer::operator=(StructuredBuffer&& structuredBuffer)
{
	this->StructuredBuffer::StructuredBuffer(std::move(structuredBuffer));
	return *this;
}

void Carol::StructuredBuffer::InitCounterResetBuffer(Heap* heap)
{
	sCounterResetBuffer = make_unique<Resource>(
		GetRvaluePtr(CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t))),
		heap,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	uint32_t zero = 0;
	sCounterResetBuffer->CopyData(&zero, sizeof(uint32_t));
}

void Carol::StructuredBuffer::ResetCounter()
{
	gCommandList->CopyBufferRegion(mResource->Get(), mCounterOffset, sCounterResetBuffer->Get(), 0, sizeof(uint32_t));
}

void Carol::StructuredBuffer::CopyElements(const void* data, uint32_t offset, uint32_t numElements)
{
	uint32_t byteSize = numElements * mElementSize;
	uint32_t byteOffset = offset * mElementSize;
	CopyData(data, byteSize, byteOffset);
}

uint32_t Carol::StructuredBuffer::GetNumElements()
{
	return mNumElements;
}

uint32_t Carol::StructuredBuffer::GetElementSize()
{
	return mElementSize;
}

uint32_t Carol::StructuredBuffer::GetCounterOffset()
{
	return mCounterOffset;
}

D3D12_GPU_VIRTUAL_ADDRESS Carol::StructuredBuffer::GetElementAddress(uint32_t offset)
{
	return mResource->GetGPUVirtualAddress() + offset * mElementSize;
}

bool Carol::StructuredBuffer::IsConstant()
{
	return mIsConstant;
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

	gDescriptorManager->CpuCbvSrvUavAllocate(1, mCpuSrvAllocInfo.get());
	gDescriptorManager->GpuCbvSrvUavAllocate(1, mGpuSrvAllocInfo.get());
	gDescriptorManager->CreateSrv(&srvDesc, mResource.get(), mCpuSrvAllocInfo.get());
}

void Carol::StructuredBuffer::BindUav()
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.Buffer.NumElements = mViewNumElements;
	uavDesc.Buffer.FirstElement = mFirstElement;
	uavDesc.Buffer.StructureByteStride = mElementSize;
	uavDesc.Buffer.CounterOffsetInBytes = mCounterOffset;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	gDescriptorManager->CpuCbvSrvUavAllocate(1, mCpuUavAllocInfo.get());
	gDescriptorManager->GpuCbvSrvUavAllocate(1, mGpuUavAllocInfo.get());
	gDescriptorManager->CreateUav(&uavDesc, mResource.get(), mResource.get(), mCpuUavAllocInfo.get());
}

void Carol::StructuredBuffer::BindRtv()
{
}

void Carol::StructuredBuffer::BindDsv()
{
}

uint32_t Carol::StructuredBuffer::AlignForConstantBuffer(uint32_t byteSize)
{
	uint32_t alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	return (byteSize + alignment - 1) & ~(alignment - 1);
}


uint32_t Carol::StructuredBuffer::AlignForUavCounter(uint32_t byteSize)
{
	uint32_t alignment = D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT;
	return (byteSize + alignment - 1) & ~(alignment - 1);
}

Carol::FastConstantBufferAllocator::FastConstantBufferAllocator(uint32_t numElements, uint32_t elementSize, Heap* heap)
	:mCurrOffset(0)
{
	mResourceQueue = make_unique<StructuredBuffer>(numElements, elementSize, heap, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE, true);
}

Carol::FastConstantBufferAllocator::FastConstantBufferAllocator(FastConstantBufferAllocator&& fastResourceAllocator)
{
	mResourceQueue = std::move(fastResourceAllocator.mResourceQueue);
	mCurrOffset = fastResourceAllocator.mCurrOffset;
}

Carol::FastConstantBufferAllocator& Carol::FastConstantBufferAllocator::operator=(FastConstantBufferAllocator&& fastResourceAllocator)
{
	this->FastConstantBufferAllocator::FastConstantBufferAllocator(std::move(fastResourceAllocator));
	return *this;
}

D3D12_GPU_VIRTUAL_ADDRESS Carol::FastConstantBufferAllocator::Allocate(const void* data)
{
	auto addr = mResourceQueue->GetElementAddress(mCurrOffset);
	mResourceQueue->CopyElements(data, mCurrOffset);
	mCurrOffset = (mCurrOffset + 1) % mResourceQueue->GetNumElements();

	return addr;
}

Carol::RawBuffer::RawBuffer(
	uint32_t byteSize,
	Heap* heap,
	D3D12_RESOURCE_STATES initState,
	D3D12_RESOURCE_FLAGS flags)
{
	mResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	mResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	mResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	mResourceDesc.Flags = flags;
	mResourceDesc.SampleDesc.Count = 1u;
	mResourceDesc.SampleDesc.Quality = 0u;
	mResourceDesc.Width = AlignForRawBuffer(byteSize);
	mResourceDesc.Height = 1u;
	mResourceDesc.DepthOrArraySize = 1ui16;
	mResourceDesc.MipLevels = 1ui16;
	mResourceDesc.Alignment = 0ui64;

	mResource = make_unique<Resource>(&mResourceDesc, heap, initState);
	BindDescriptors();
}

Carol::RawBuffer::RawBuffer(RawBuffer&& rawBuffer)
	:Buffer(std::move(rawBuffer))
{
}

Carol::RawBuffer& Carol::RawBuffer::operator=(RawBuffer&& rawBuffer)
{
	this->RawBuffer::RawBuffer(std::move(rawBuffer));
	return *this;
}

void Carol::RawBuffer::BindSrv()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.NumElements = mResourceDesc.Width  / sizeof(uint32_t);
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.StructureByteStride = 0;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

	gDescriptorManager->CpuCbvSrvUavAllocate(1, mCpuSrvAllocInfo.get());
	gDescriptorManager->GpuCbvSrvUavAllocate(1, mGpuSrvAllocInfo.get());
	gDescriptorManager->CreateSrv(&srvDesc, mResource.get(), mCpuSrvAllocInfo.get());
}

void Carol::RawBuffer::BindUav()
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	uavDesc.Buffer.NumElements = mResourceDesc.Width / sizeof(uint32_t);
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.StructureByteStride = 0;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

	gDescriptorManager->CpuCbvSrvUavAllocate(1, mCpuUavAllocInfo.get());
	gDescriptorManager->GpuCbvSrvUavAllocate(1, mGpuUavAllocInfo.get());
	gDescriptorManager->CreateUav(&uavDesc, mResource.get(), nullptr, mCpuUavAllocInfo.get());
}

void Carol::RawBuffer::BindRtv()
{

}

void Carol::RawBuffer::BindDsv()
{

}

uint32_t Carol::RawBuffer::AlignForRawBuffer(uint32_t byteSize)
{
	return (byteSize + 3) & (~3);
}

