#include <scene/texture.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/descriptor_allocator.h>
#include <dx12/root_signature.h>
#include <utils/common.h>
#include <DirectXTex.h>
#include <memory>
#include <vector>

namespace Carol {
	using std::vector;
	using std::wstring;
	using std::unordered_map;
	using std::make_unique;
	using std::make_shared;
	using namespace DirectX;
	
}

Carol::DefaultResource* Carol::Texture::GetResource()
{
	return mTexture.get();
}

void Carol::Texture::SetDesc()
{
	auto texResDesc = mTexture->Get()->GetDesc();
	mTexDesc = {};

	mTexDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	switch (texResDesc.Dimension)
	{
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
		mTexDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
		mTexDesc.Texture1D.MipLevels = texResDesc.MipLevels;
		mTexDesc.Texture1D.MostDetailedMip = 0;
		mTexDesc.Texture1D.ResourceMinLODClamp = 0.0f;
		break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		if (mIsCube)
		{
			mTexDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			mTexDesc.TextureCube.MipLevels = texResDesc.MipLevels;
			mTexDesc.TextureCube.MostDetailedMip = 0;
			mTexDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		}
		else
		{
			mTexDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			mTexDesc.Texture2D.MipLevels = texResDesc.MipLevels;
			mTexDesc.Texture2D.MostDetailedMip = 0;
			mTexDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		}
		break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		mTexDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
		mTexDesc.Texture3D.MipLevels = texResDesc.MipLevels;
		mTexDesc.Texture3D.MostDetailedMip = 0;
		mTexDesc.Texture3D.ResourceMinLODClamp = 0.0f;
		break;
	}
}

D3D12_SHADER_RESOURCE_VIEW_DESC Carol::Texture::GetDesc()
{
	return mTexDesc;
}

void Carol::Texture::LoadTexture(ID3D12GraphicsCommandList* cmdList, Heap* texHeap, Heap* uploadHeap, wstring fileName, bool isSrgb)
{
	wstring suffix = fileName.substr(fileName.find_last_of(L'.') + 1, 3);
	TexMetadata metaData;
	ScratchImage scratchImage;

	if (suffix == L"dds")
	{
		ThrowIfFailed(LoadFromDDSFile(fileName.c_str(), DirectX::DDS_FLAGS_FORCE_RGB, &metaData, scratchImage));
	}
	else if (suffix == L"tga")
	{
		ThrowIfFailed(DirectX::LoadFromTGAFile(fileName.c_str(), DirectX::TGA_FLAGS_FORCE_LINEAR, &metaData, scratchImage));
	}
	else
	{
		ThrowIfFailed(LoadFromWICFile(fileName.c_str(), DirectX::WIC_FLAGS_FORCE_RGB, &metaData, scratchImage));
	}

	if (isSrgb)
	{
		metaData.format = DirectX::MakeSRGB(metaData.format);
	}

	mIsCube = metaData.IsCubemap();
	mIsVolume = metaData.IsVolumemap();

	D3D12_RESOURCE_DESC texResDesc = {};
	mTexDesc = {};
	
	switch (metaData.dimension)
	{
	case DirectX::TEX_DIMENSION_TEXTURE1D:
		texResDesc = CD3DX12_RESOURCE_DESC::Tex1D(metaData.format, static_cast<UINT64>(metaData.width), static_cast<UINT16>(metaData.arraySize), static_cast<UINT16>(metaData.mipLevels));	
		break;
	case DirectX::TEX_DIMENSION_TEXTURE2D:
		texResDesc = CD3DX12_RESOURCE_DESC::Tex2D(metaData.format, static_cast<UINT64>(metaData.width), metaData.height, static_cast<UINT16>(metaData.arraySize), static_cast<UINT16>(metaData.mipLevels));
		break;
	case DirectX::TEX_DIMENSION_TEXTURE3D:
		texResDesc = CD3DX12_RESOURCE_DESC::Tex3D(metaData.format, static_cast<UINT64>(metaData.width), metaData.height, static_cast<UINT16>(metaData.depth), static_cast<UINT16>(metaData.mipLevels));
		break;

	}
	
	mTexture = make_unique<DefaultResource>(&texResDesc, texHeap);
	SetDesc();

	vector<D3D12_SUBRESOURCE_DATA> subresources(scratchImage.GetImageCount());
	const Image* images = scratchImage.GetImages();

	for (int i=0;i<subresources.size();++i)
	{
		subresources[i].RowPitch = images[i].rowPitch;
		subresources[i].SlicePitch = images[i].slicePitch;
		subresources[i].pData = images[i].pixels;
	}

	mTexture->CopySubresources(cmdList, uploadHeap, subresources.data(), 0, subresources.size());
}

void Carol::Texture::ReleaseIntermediateBuffer()
{
	mTexture->ReleaseIntermediateBuffer();
}

Carol::TextureManager::TextureManager(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	Heap* texHeap, Heap* uploadHeap,
	RootSignature* rootSignature,
	DescriptorAllocator* allocator,
	uint32_t numFrames)
	:mDevice(device),
	mCommandList(cmdList),
	mTexturesHeap(texHeap),
	mUploadBuffersHeap(uploadHeap),
	mRootSignature(rootSignature),
	mDescriptorAllocator(allocator),
	mGpuTex1DAllocInfo(numFrames),
	mGpuTex2DAllocInfo(numFrames),
	mGpuTex3DAllocInfo(numFrames),
	mGpuTexCubeAllocInfo(numFrames)
{
	for (int i = 0; i < numFrames; ++i)
	{
		mGpuTex1DAllocInfo[i] = make_unique<DescriptorAllocInfo>();
		mGpuTex2DAllocInfo[i] = make_unique<DescriptorAllocInfo>();
		mGpuTex3DAllocInfo[i] = make_unique<DescriptorAllocInfo>();
		mGpuTexCubeAllocInfo[i] = make_unique<DescriptorAllocInfo>();
	}
}

Carol::TextureManager::~TextureManager()
{
	for (int i = 0; i < mGpuTex1DAllocInfo.size(); ++i)
	{
		if (mGpuTex1DAllocInfo[i]->Allocator)
		{
			mDescriptorAllocator->GpuDeallocate(mGpuTex1DAllocInfo[i].get());
		}

		if (mGpuTex2DAllocInfo[i]->Allocator)
		{
			mDescriptorAllocator->GpuDeallocate(mGpuTex2DAllocInfo[i].get());
		}

		if (mGpuTex3DAllocInfo[i]->Allocator)
		{
			mDescriptorAllocator->GpuDeallocate(mGpuTex3DAllocInfo[i].get());
		}

		if (mGpuTexCubeAllocInfo[i]->Allocator)
		{
			mDescriptorAllocator->GpuDeallocate(mGpuTexCubeAllocInfo[i].get());
		}
	}
}

void Carol::TextureManager::LoadTexture(const wstring& fileName, bool isSrgb)
{
	if (fileName.size() == 0)
	{
		return;
	}

	if (mTextures.count(fileName) == 0)
	{
		mTextures[fileName] = make_unique<AllocatedTexture>(make_unique<Texture>(),make_unique<DescriptorAllocInfo>());
		auto& tex = mTextures[fileName]->Texture;
		auto& info = mTextures[fileName]->CpuDescriptorAllocInfo;
		
		tex->LoadTexture(mCommandList, mTexturesHeap, mUploadBuffersHeap, fileName);
		mDescriptorAllocator->CpuAllocate(1, info.get()); auto a = mDescriptorAllocator->GetCpuHandle(info.get());
		mDevice->CreateShaderResourceView(tex->GetResource()->Get(), GetRvaluePtr(tex->GetDesc()), mDescriptorAllocator->GetCpuHandle(info.get()));
	}
	else
	{
		++mTextures[fileName]->NumRef;
	}
}

void Carol::TextureManager::UnloadTexture(const wstring& fileName)
{
	--mTextures[fileName]->NumRef;
	if (mTextures[fileName]->NumRef == 0)
	{
		mTextures.erase(fileName);
	}
}

void Carol::TextureManager::ReleaseIntermediateBuffers(const std::wstring& fileName)
{
	if (mTextures.count(fileName) == 0)
	{
		return;
	}

	mTextures[fileName]->Texture->ReleaseIntermediateBuffer();
}

void Carol::TextureManager::ClearGpuTextures(uint32_t currFrame)
{
	mGpuTexIdx.clear();

	mGpuTex1DCount = 0;
	mGpuTex2DCount = 0;
	mGpuTex3DCount = 0;
	mGpuTexCubeCount = 0;

	mGpuTex1D.clear();
	mGpuTex2D.clear();
	mGpuTex3D.clear();
	mGpuTexCube.clear();

	if (mGpuTex1DAllocInfo[currFrame]->Allocator)
	{
		mDescriptorAllocator->GpuDeallocate(mGpuTex1DAllocInfo[currFrame].get());
	}

	if (mGpuTex2DAllocInfo[currFrame]->Allocator)
	{
		mDescriptorAllocator->GpuDeallocate(mGpuTex2DAllocInfo[currFrame].get());
	}

	if (mGpuTex3DAllocInfo[currFrame]->Allocator)
	{
		mDescriptorAllocator->GpuDeallocate(mGpuTex3DAllocInfo[currFrame].get());
	}

	if (mGpuTexCubeAllocInfo[currFrame]->Allocator)
	{
		mDescriptorAllocator->GpuDeallocate(mGpuTexCubeAllocInfo[currFrame].get());
	}
}

uint32_t Carol::TextureManager::CollectGpuTextures(const wstring& fileName)
{
	static auto gpuCollect = [](vector<wstring>& tex, const wstring& fileName, uint32_t& count, unordered_map<wstring, uint32_t>& idxMap)
	{
		tex.push_back(fileName);
		uint32_t temp = count;
		idxMap[fileName] = count;
		++count;
		return temp;
	};

	if (mTextures.count(fileName) == 0)
	{
		return -1;
	}

	if (mGpuTexIdx.count(fileName) == 0)
	{
		auto desc = mTextures[fileName]->Texture->GetDesc();
		auto* cpuInfo = mTextures[fileName]->CpuDescriptorAllocInfo.get();
		uint32_t temp;

		switch (desc.ViewDimension)
		{
		case D3D12_SRV_DIMENSION_TEXTURE1D:
			return gpuCollect(mGpuTex1D, fileName, mGpuTex1DCount, mGpuTexIdx);
		case D3D12_SRV_DIMENSION_TEXTURE2D:
			return gpuCollect(mGpuTex2D, fileName, mGpuTex2DCount, mGpuTexIdx);
		case D3D12_SRV_DIMENSION_TEXTURE3D:
			return gpuCollect(mGpuTex3D, fileName, mGpuTex3DCount, mGpuTexIdx);
		case D3D12_SRV_DIMENSION_TEXTURECUBE:
			return gpuCollect(mGpuTexCube, fileName, mGpuTexCubeCount, mGpuTexIdx);
		}
	}
	else
	{
		return mGpuTexIdx[fileName];
	}
}

void Carol::TextureManager::AllocateGpuTextures(uint32_t currFrame)
{
	auto allocate = [&](uint32_t count, vector<wstring>& tex, DescriptorAllocator* allocator, DescriptorAllocInfo* info)
	{
		if (count)
		{
			allocator->GpuAllocate(count, info);
			CopyDescriptors(tex, info);
		}
	};

	allocate(mGpuTex1DCount, mGpuTex1D, mDescriptorAllocator, mGpuTex1DAllocInfo[currFrame].get());
	allocate(mGpuTex2DCount, mGpuTex2D, mDescriptorAllocator, mGpuTex2DAllocInfo[currFrame].get());
	allocate(mGpuTex3DCount, mGpuTex3D, mDescriptorAllocator, mGpuTex3DAllocInfo[currFrame].get());
	allocate(mGpuTexCubeCount, mGpuTexCube, mDescriptorAllocator, mGpuTexCubeAllocInfo[currFrame].get());
}

uint32_t Carol::TextureManager::GetNumTex1D()
{
	return mGpuTex1DCount;
}

uint32_t Carol::TextureManager::GetNumTex2D()
{
	return mGpuTex2DCount;
}

uint32_t Carol::TextureManager::GetNumTex3D()
{
	return mGpuTex3DCount;
}

uint32_t Carol::TextureManager::GetNumTexCube()
{
	return mGpuTexCubeCount;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::TextureManager::GetTex1DHandle(uint32_t currFrame)
{
	return mDescriptorAllocator->GetShaderGpuHandle(mGpuTex1DAllocInfo[currFrame].get());
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::TextureManager::GetTex2DHandle(uint32_t currFrame)
{
	return mDescriptorAllocator->GetShaderGpuHandle(mGpuTex2DAllocInfo[currFrame].get());
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::TextureManager::GetTex3DHandle(uint32_t currFrame)
{
	return mDescriptorAllocator->GetShaderGpuHandle(mGpuTex3DAllocInfo[currFrame].get());
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::TextureManager::GetTexCubeHandle(uint32_t currFrame)
{
	return mDescriptorAllocator->GetShaderGpuHandle(mGpuTexCubeAllocInfo[currFrame].get());
}

void Carol::TextureManager::CopyDescriptors(std::vector<std::wstring>& tex, DescriptorAllocInfo* info)
{
	auto gpuHandle = mDescriptorAllocator->GetShaderCpuHandle(info);
	
	for (auto& fileName : tex)
	{
		auto cpuHandle = mDescriptorAllocator->GetCpuHandle(mTextures[fileName]->CpuDescriptorAllocInfo.get());
		mDevice->CopyDescriptorsSimple(1, gpuHandle, cpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gpuHandle.Offset(1, mDescriptorAllocator->GetDescriptorSize());
	}
}
