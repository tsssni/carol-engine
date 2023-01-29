export module carol.renderer.dx12.resource;
import carol.renderer.dx12.command;
import carol.renderer.dx12.heap;
import carol.renderer.dx12.descriptor;
import carol.renderer.utils;
import <wrl/client.h>;
import <memory>;
import <span>;

namespace Carol
{
    using Microsoft::WRL::ComPtr;
    using std::make_unique;
    using std::span;
    using std::unique_ptr;
    using std::vector;

    export DXGI_FORMAT GetBaseFormat(DXGI_FORMAT format)
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

	export DXGI_FORMAT GetUavFormat(DXGI_FORMAT format)
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

	export DXGI_FORMAT GetDsvFormat(DXGI_FORMAT format)
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

	export uint32_t GetPlaneSize(DXGI_FORMAT format)
    {
        switch (format)
        {
        case DXGI_FORMAT_NV12:
            return 2;
        default:
            return 1;
        }
    }

	export class Resource
	{
	public:
		Resource()
        {

        }

		Resource(
			D3D12_RESOURCE_DESC* desc,
			Heap* heap,
			D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_COMMON,
			D3D12_CLEAR_VALUE* optimizedClearValue = nullptr)
        {
            mHeapAllocInfo = heap->Allocate(desc);

            gDevice->CreatePlacedResource(
                heap->GetHeap(mHeapAllocInfo.get()),
                heap->GetOffset(mHeapAllocInfo.get()),
                desc,
                initState,
                optimizedClearValue,
                IID_PPV_ARGS(mResource.GetAddressOf()));

            mHeapAllocInfo->Resource = mResource;
        }

		virtual ~Resource()
        {
            if (mHeapAllocInfo && gHeapManager)
            {
                mMappedData = nullptr;
                mHeapAllocInfo->Heap->Deallocate(std::move(mHeapAllocInfo));
            }
        }

		ID3D12Resource* Get()const
        {
            return mResource.Get();
        }

		ID3D12Resource** GetAddressOf()
        {
            return mResource.GetAddressOf();
        }

		byte* GetMappedData()const
        {
            return mMappedData;
        }

		Heap* GetHeap()const
        {
            return mHeapAllocInfo->Heap;
        }

		D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress()const
        {
            return mResource->GetGPUVirtualAddress();
        }

		void CopySubresources(
			Heap* intermediateHeap,
			const void* data,
			uint32_t byteSize,
			D3D12_RESOURCE_STATES beforeState
		)
        {
            D3D12_SUBRESOURCE_DATA subresource;
            subresource.RowPitch = byteSize;
            subresource.SlicePitch = subresource.RowPitch;
            subresource.pData = data;

            CopySubresources(intermediateHeap, &subresource, 0, 1, beforeState);
        }

		void CopySubresources(
			Heap* intermediateHeap,
			D3D12_SUBRESOURCE_DATA* subresources,
			uint32_t firstSubresource,
			uint32_t numSubresources,
			D3D12_RESOURCE_STATES beforeState
		)
        {
            gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mResource.Get(), beforeState, D3D12_RESOURCE_STATE_COPY_DEST)));
            auto resourceSize = GetRequiredIntermediateSize(mResource.Get(), firstSubresource, numSubresources);

            auto intermediateResourcedesc = CD3DX12_RESOURCE_DESC::Buffer(resourceSize);
            mIntermediateBufferAllocInfo = intermediateHeap->Allocate(&intermediateResourcedesc);
            
            gDevice->CreatePlacedResource(
                intermediateHeap->GetHeap(mIntermediateBufferAllocInfo.get()),
                intermediateHeap->GetOffset(mIntermediateBufferAllocInfo.get()),
                &intermediateResourcedesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(mIntermediateBuffer.GetAddressOf()));

            mIntermediateBufferAllocInfo->Resource = mIntermediateBuffer;

            UpdateSubresources(gCommandList.Get(), mResource.Get(), mIntermediateBuffer.Get(), 0, firstSubresource, numSubresources, subresources);
            gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, beforeState)));
        }

		void CopyData(const void* data, uint32_t byteSize, uint32_t offset = 0)
        {
            if (!mMappedData)
            {
                ThrowIfFailed(mResource->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));
                mHeapAllocInfo->MappedData = mMappedData;
            }

            memcpy(mMappedData + offset, data, byteSize);
        }

		void ReleaseIntermediateBuffer()
        {
            if (mIntermediateBuffer)
            {
                mIntermediateBufferAllocInfo->Heap->DeleteResourceImmediate(std::move(mIntermediateBufferAllocInfo));
                mIntermediateBuffer.Reset();
            }
        }

	protected:
		ComPtr<ID3D12Resource> mResource;
		unique_ptr<HeapAllocInfo> mHeapAllocInfo;

		ComPtr<ID3D12Resource> mIntermediateBuffer;
		unique_ptr<HeapAllocInfo> mIntermediateBufferAllocInfo;

		byte* mMappedData = nullptr;
	};

	export class Buffer
	{
	public:
		Buffer()
        {

        }

		Buffer(Buffer&& buffer)
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

		Buffer& operator=(Buffer&& buffer)
        {
            this->Buffer::Buffer(std::move(buffer));
            return *this;
        }

		~Buffer()
        {
            static auto cpuCbvSrvUavDeallocate = [](unique_ptr<DescriptorAllocInfo>& info)
            {
                if (info && gDescriptorManager)
                {
                    gDescriptorManager->CpuCbvSrvUavDeallocate(std::move(info));
                }
            };

            static auto gpuCbvSrvUavDeallocate = [](unique_ptr<DescriptorAllocInfo>& info)
            {
                if (info && gDescriptorManager)
                {
                    gDescriptorManager->GpuCbvSrvUavDeallocate(std::move(info));
                }
            };

            static auto rtvDeallocate = [](unique_ptr<DescriptorAllocInfo>& info)
            {
                if (info)
                {
                    gDescriptorManager->RtvDeallocate(std::move(info));
                }
            };

            static auto dsvDeallocate = [](unique_ptr<DescriptorAllocInfo>& info)
            {
                if (info)
                {
                    gDescriptorManager->DsvDeallocate(std::move(info));
                }
            };

            cpuCbvSrvUavDeallocate(mCpuCbvAllocInfo);
            gpuCbvSrvUavDeallocate(mGpuCbvAllocInfo);
            cpuCbvSrvUavDeallocate(mCpuSrvAllocInfo);
            gpuCbvSrvUavDeallocate(mGpuSrvAllocInfo);
            cpuCbvSrvUavDeallocate(mCpuUavAllocInfo);
            gpuCbvSrvUavDeallocate(mGpuUavAllocInfo);
            rtvDeallocate(mRtvAllocInfo);
            dsvDeallocate(mDsvAllocInfo);
        }

		ID3D12Resource* Get()const
        {
            return mResource->Get();
        }

		ID3D12Resource** GetAddressOf()const
        {
            return mResource->GetAddressOf();
        }

		D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress()const
        {
            return mResource->GetGPUVirtualAddress();
        }

		void CopySubresources(
			Heap* intermediateHeap,
			const void* data,
			uint32_t byteSize,
			D3D12_RESOURCE_STATES beforeState
		)
        {
            mResource->CopySubresources(intermediateHeap, data, byteSize, beforeState);
        }

		void CopySubresources(
			Heap* intermediateHeap,
			D3D12_SUBRESOURCE_DATA* subresources,
			uint32_t firstSubresource,
			uint32_t numSubresources,
			D3D12_RESOURCE_STATES beforeState
		)
        {
            mResource->CopySubresources(intermediateHeap, subresources, firstSubresource, numSubresources, beforeState);
        }

		void CopyData(const void* data, uint32_t byteSize, uint32_t offset = 0)
        {
            mResource->CopyData(data, byteSize, offset);
        }

		void ReleaseIntermediateBuffer()
        {
            mResource->ReleaseIntermediateBuffer();
        }

		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuSrv(uint32_t planeSlice = 0)const
        {
            return gDescriptorManager->GetCpuCbvSrvUavHandle(mCpuSrvAllocInfo.get(), planeSlice);
        }

		D3D12_CPU_DESCRIPTOR_HANDLE GetShaderCpuSrv(uint32_t planeSlice = 0)const
        {
            return gDescriptorManager->GetShaderCpuCbvSrvUavHandle(mGpuSrvAllocInfo.get(), planeSlice);
        }

		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuSrv(uint32_t planeSlice = 0)const
        {
            return gDescriptorManager->GetGpuCbvSrvUavHandle(mGpuSrvAllocInfo.get(), planeSlice);
        }

		uint32_t GetGpuSrvIdx(uint32_t planeSlice = 0)const
        {
            return mGpuSrvAllocInfo->StartOffset + planeSlice;
        }

		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuUav(uint32_t mipSlice = 0, uint32_t planeSlice = 0)const
        {
            return gDescriptorManager->GetCpuCbvSrvUavHandle(mCpuUavAllocInfo.get(), mipSlice + planeSlice * mResourceDesc.MipLevels);
        }

		D3D12_CPU_DESCRIPTOR_HANDLE GetShaderCpuUav(uint32_t mipSlice = 0, uint32_t planeSlice = 0)const
        {
            return gDescriptorManager->GetShaderCpuCbvSrvUavHandle(mGpuUavAllocInfo.get(), mipSlice + planeSlice * mResourceDesc.MipLevels);
        }

		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuUav(uint32_t mipSlice = 0, uint32_t planeSlice = 0)const
        {
            return gDescriptorManager->GetGpuCbvSrvUavHandle(mGpuUavAllocInfo.get(), mipSlice + planeSlice * mResourceDesc.MipLevels);
        }

		uint32_t GetGpuUavIdx(uint32_t mipSlice = 0, uint32_t planeSlice = 0)const
        {
            return mGpuUavAllocInfo->StartOffset + mipSlice + planeSlice * mResourceDesc.MipLevels;
        }

		D3D12_CPU_DESCRIPTOR_HANDLE GetRtv(uint32_t mipSlice = 0, uint32_t planeSlice = 0)const
        {
            return gDescriptorManager->GetRtvHandle(mRtvAllocInfo.get(), mipSlice + planeSlice * mResourceDesc.MipLevels);
        }

		D3D12_CPU_DESCRIPTOR_HANDLE GetDsv(uint32_t mipSlice = 0)const
        {
            return gDescriptorManager->GetDsvHandle(mDsvAllocInfo.get(), mipSlice);
        }

	protected:
		void BindDescriptors()
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
        }

		virtual void BindSrv() = 0;
		virtual void BindUav() = 0;
		virtual void BindRtv() = 0;
		virtual void BindDsv() = 0;

		virtual void CreateCbvs(span<const D3D12_CONSTANT_BUFFER_VIEW_DESC> cbvDescs)
        {
            mCpuCbvAllocInfo = gDescriptorManager->CpuCbvSrvUavAllocate(cbvDescs.size());
            mGpuCbvAllocInfo = gDescriptorManager->GpuCbvSrvUavAllocate(cbvDescs.size());

            for (int i = 0; i < cbvDescs.size(); ++i)
            {
                gDevice->CreateConstantBufferView(&cbvDescs[i], gDescriptorManager->GetCpuCbvSrvUavHandle(mCpuCbvAllocInfo.get(), i));
            }

            gDevice->CopyDescriptorsSimple(
                cbvDescs.size(),
                gDescriptorManager->GetShaderCpuCbvSrvUavHandle(mGpuCbvAllocInfo.get()),
                gDescriptorManager->GetCpuCbvSrvUavHandle(mCpuCbvAllocInfo.get()),
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

		virtual void CreateSrvs(span<const D3D12_SHADER_RESOURCE_VIEW_DESC> srvDescs)
        {
            mCpuSrvAllocInfo = gDescriptorManager->CpuCbvSrvUavAllocate(srvDescs.size());
            mGpuSrvAllocInfo = gDescriptorManager->GpuCbvSrvUavAllocate(srvDescs.size());

            for (int i = 0; i < srvDescs.size(); ++i)
            {
                gDevice->CreateShaderResourceView(mResource->Get(), &srvDescs[i], gDescriptorManager->GetCpuCbvSrvUavHandle(mCpuSrvAllocInfo.get(), i));
            }

            gDevice->CopyDescriptorsSimple(
                srvDescs.size(),
                gDescriptorManager->GetShaderCpuCbvSrvUavHandle(mGpuSrvAllocInfo.get()),
                gDescriptorManager->GetCpuCbvSrvUavHandle(mCpuSrvAllocInfo.get()),
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

		virtual void CreateUavs(span<const D3D12_UNORDERED_ACCESS_VIEW_DESC> uavDescs, bool counter)
        {
            mCpuUavAllocInfo = gDescriptorManager->CpuCbvSrvUavAllocate(uavDescs.size());
            mGpuUavAllocInfo = gDescriptorManager->GpuCbvSrvUavAllocate(uavDescs.size());

            for (int i = 0; i < uavDescs.size(); ++i)
            {
                gDevice->CreateUnorderedAccessView(mResource->Get(), counter ? mResource->Get() : nullptr, &uavDescs[i], gDescriptorManager->GetCpuCbvSrvUavHandle(mCpuUavAllocInfo.get(), i));
            }

            gDevice->CopyDescriptorsSimple(
                uavDescs.size(),
                gDescriptorManager->GetShaderCpuCbvSrvUavHandle(mGpuUavAllocInfo.get()),
                gDescriptorManager->GetCpuCbvSrvUavHandle(mCpuUavAllocInfo.get()),
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

		virtual void CreateRtvs(span<const D3D12_RENDER_TARGET_VIEW_DESC> rtvDescs)
        {
            mRtvAllocInfo = gDescriptorManager->RtvAllocate(rtvDescs.size());

            for (int i = 0; i < rtvDescs.size(); ++i)
            {
                gDevice->CreateRenderTargetView(mResource->Get(), &rtvDescs[i], gDescriptorManager->GetRtvHandle(mRtvAllocInfo.get(), i));
            }
        }

		virtual void CreateDsvs(span<const D3D12_DEPTH_STENCIL_VIEW_DESC> dsvDescs)
        {
            mDsvAllocInfo = gDescriptorManager->DsvAllocate(dsvDescs.size());

            for (int i = 0; i < dsvDescs.size(); ++i)
            {
                gDevice->CreateDepthStencilView(mResource->Get(), &dsvDescs[i], gDescriptorManager->GetDsvHandle(mDsvAllocInfo.get(), i));
            }
        }

		unique_ptr<Resource> mResource;
		D3D12_RESOURCE_DESC mResourceDesc = {};

		unique_ptr<DescriptorAllocInfo> mCpuCbvAllocInfo;
		unique_ptr<DescriptorAllocInfo> mGpuCbvAllocInfo;

		unique_ptr<DescriptorAllocInfo> mCpuSrvAllocInfo;
		unique_ptr<DescriptorAllocInfo> mGpuSrvAllocInfo;
		
		unique_ptr<DescriptorAllocInfo> mCpuUavAllocInfo;
		unique_ptr<DescriptorAllocInfo> mGpuUavAllocInfo;

		unique_ptr<DescriptorAllocInfo> mRtvAllocInfo;
		unique_ptr<DescriptorAllocInfo> mDsvAllocInfo;
	};

	export enum ColorBufferViewDimension
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

	export class ColorBuffer : public Buffer
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
			uint32_t viewMipLevels = 0,
			uint32_t mostDetailedMip = 0,
			float resourceMinLODClamp = 0.f,
			uint32_t viewArraySize = 0,
			uint32_t firstArraySlice = 0,
			uint32_t sampleCount = 1,
			uint32_t sampleQuality = 0)
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

		ColorBuffer(ColorBuffer&& colorBuffer)
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

		ColorBuffer& operator=(ColorBuffer&& colorBuffer)
        {
            this->ColorBuffer::ColorBuffer(std::move(colorBuffer));
            return *this;
        }

	protected:
		virtual void BindSrv()override
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

            CreateSrvs(srvDescs);
        }

		virtual void BindUav()override
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

            CreateUavs(uavDescs, false);
        }

		virtual void BindRtv()override
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
            
            CreateRtvs(rtvDescs);
        }

		virtual void BindDsv()override
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

            CreateDsvs(dsvDescs);
        }

		D3D12_RESOURCE_DIMENSION GetResourceDimension(ColorBufferViewDimension viewDimension)const
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

		ColorBufferViewDimension mViewDimension;
		DXGI_FORMAT mFormat;

		uint32_t mViewMipLevels = 1;
		uint32_t mMostDetailedMip = 0;
		float mResourceMinLODClamp = 0.f;

		uint32_t mViewArraySize = 0;
		uint32_t mFirstArraySlice = 0;
		uint32_t mPlaneSize = 0;
	};

	export class StructuredBuffer : public Buffer
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
			uint32_t firstElement = 0)
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

		StructuredBuffer(StructuredBuffer&& structuredBuffer)
        	:Buffer(std::move(structuredBuffer))
        {
            mNumElements = structuredBuffer.mNumElements;
            mElementSize = structuredBuffer.mElementSize;
            mViewNumElements = structuredBuffer.mViewNumElements;
            mFirstElement = structuredBuffer.mFirstElement;
        }

		StructuredBuffer& operator=(StructuredBuffer&& structuredBuffer)
        {
            this->StructuredBuffer::StructuredBuffer(std::move(structuredBuffer));
            return *this;
        }

		static void InitCounterResetBuffer(Heap* heap)
        {
            sCounterResetBuffer = make_unique<Resource>(
                GetRvaluePtr(CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t))),
                heap,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

            uint32_t zero = 0;
            sCounterResetBuffer->CopyData(&zero, sizeof(uint32_t));
        }

		void ResetCounter()
        {
            gCommandList->CopyBufferRegion(mResource->Get(), mCounterOffset, sCounterResetBuffer->Get(), 0, sizeof(uint32_t));
        }

		void CopyElements(const void* data, uint32_t offset = 0, uint32_t numElements = 1)
        {
            uint32_t byteSize = numElements * mElementSize;
            uint32_t byteOffset = offset * mElementSize;
            CopyData(data, byteSize, byteOffset);
        }

		uint32_t GetNumElements()const
        {
            return mNumElements;
        }

		uint32_t GetElementSize()const
        {
            return mElementSize;
        }

		uint32_t GetCounterOffset()const
        {
            return mCounterOffset;
        }

		D3D12_GPU_VIRTUAL_ADDRESS GetElementAddress(uint32_t offset)const
        {
            return mResource->GetGPUVirtualAddress() + offset * mElementSize;
        }

		bool IsConstant()const
        {
            return mIsConstant;
        }

	protected:
		virtual void BindSrv()override
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.NumElements = mViewNumElements;
            srvDesc.Buffer.FirstElement = mFirstElement;
            srvDesc.Buffer.StructureByteStride = mElementSize;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

            CreateSrvs(span(&srvDesc, 1));
        }

		virtual void BindUav()override
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Format = DXGI_FORMAT_UNKNOWN;
            uavDesc.Buffer.NumElements = mViewNumElements;
            uavDesc.Buffer.FirstElement = mFirstElement;
            uavDesc.Buffer.StructureByteStride = mElementSize;
            uavDesc.Buffer.CounterOffsetInBytes = mCounterOffset;
            uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

            CreateUavs(span(&uavDesc, 1), true);
        }

		virtual void BindRtv()override{}

		virtual void BindDsv()override{}

		uint32_t AlignForConstantBuffer(uint32_t byteSize)const
        {
            uint32_t alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
            return (byteSize + alignment - 1) & ~(alignment - 1);
        }

		uint32_t AlignForUavCounter(uint32_t byteSize)const
        {
            uint32_t alignment = D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT;
            return (byteSize + alignment - 1) & ~(alignment - 1);
        }

		uint32_t mNumElements = 0;
		uint32_t mElementSize = 0;
		bool mIsConstant = false;

		uint32_t mCounterOffset = 0;
		uint32_t mViewNumElements = 0;
		uint32_t mFirstElement = 0;

		static unique_ptr<Resource> sCounterResetBuffer;
	};

	export class FastConstantBufferAllocator
	{
	public:
		FastConstantBufferAllocator(
			uint32_t numElements,
			uint32_t elementSize,
			Heap* heap)
            	:mCurrOffset(0)
        {
            mResourceQueue = make_unique<StructuredBuffer>(numElements, elementSize, heap, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE, true);
        }

		FastConstantBufferAllocator(FastConstantBufferAllocator&& fastResourceAllocator)
        {
            mResourceQueue = std::move(fastResourceAllocator.mResourceQueue);
            mCurrOffset = fastResourceAllocator.mCurrOffset;
        }

		FastConstantBufferAllocator& operator=(FastConstantBufferAllocator&& fastResourceAllocator)
        {
            this->FastConstantBufferAllocator::FastConstantBufferAllocator(std::move(fastResourceAllocator));
            return *this;
        }

		D3D12_GPU_VIRTUAL_ADDRESS Allocate(const void* data)
        {
            auto addr = mResourceQueue->GetElementAddress(mCurrOffset);
            mResourceQueue->CopyElements(data, mCurrOffset);
            mCurrOffset = (mCurrOffset + 1) % mResourceQueue->GetNumElements();

            return addr;
        }

	protected:
		unique_ptr<StructuredBuffer> mResourceQueue;
		uint32_t mCurrOffset;
	};
	
	export class RawBuffer : public Buffer
	{
	public:
		RawBuffer(
			uint32_t byteSize,
			Heap* heap,
			D3D12_RESOURCE_STATES initState,
			D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
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

		RawBuffer(RawBuffer&& rawBuffer)
        	:Buffer(std::move(rawBuffer))
        {
        }

		RawBuffer& operator=(RawBuffer&& rawBuffer)
        {
            this->RawBuffer::RawBuffer(std::move(rawBuffer));
            return *this;
        }

	protected:
		virtual void BindSrv()override
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.NumElements = mResourceDesc.Width  / sizeof(uint32_t);
            srvDesc.Buffer.FirstElement = 0;
            srvDesc.Buffer.StructureByteStride = 0;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

            CreateSrvs(span(&srvDesc, 1));
        }

		virtual void BindUav()override
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            uavDesc.Buffer.NumElements = mResourceDesc.Width / sizeof(uint32_t);
            uavDesc.Buffer.FirstElement = 0;
            uavDesc.Buffer.StructureByteStride = 0;
            uavDesc.Buffer.CounterOffsetInBytes = 0;
            uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

            CreateUavs(span(&uavDesc, 1), false);
        }

		virtual void BindRtv()override{}

		virtual void BindDsv()override{}

		uint32_t AlignForRawBuffer(uint32_t byteSize)const
        {
            return (byteSize + 3) & (~3);
        }
	};

    Carol::unique_ptr<Carol::Resource> Carol::StructuredBuffer::sCounterResetBuffer = nullptr;
}