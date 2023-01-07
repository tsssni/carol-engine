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

Carol::Texture::Texture(wstring fileName, bool isSrgb, ID3D12GraphicsCommandList* cmdList, Heap* texHeap, Heap* uploadHeap, DescriptorAllocator* allocator)
	:mNumRef(1)
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

	ColorBufferViewDimension viewDimension;
	uint32_t depthOrArraySize = metaData.arraySize;

	switch (metaData.dimension)
	{
	case DirectX::TEX_DIMENSION_TEXTURE1D:
	{
		if (metaData.arraySize == 1)
		{
			viewDimension = COLOR_BUFFER_VIEW_DIMENSION_TEXTURE1D;
		}
		else
		{
			viewDimension = COLOR_BUFFER_VIEW_DIMENSION_TEXTURE1DARRAY;
		}
		
		break;
	}

	case DirectX::TEX_DIMENSION_TEXTURE2D:
	{
		if (metaData.arraySize == 1 && !metaData.IsCubemap())
		{
			viewDimension = COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D;
		}
		else if (metaData.arraySize > 1 && !metaData.IsCubemap())
		{
			viewDimension = COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2DARRAY;
		}
		else if (metaData.arraySize == 6 && metaData.IsCubemap())
		{
			viewDimension = COLOR_BUFFER_VIEW_DIMENSION_TEXTURECUBE;
		}
		else if (metaData.arraySize > 6 && metaData.IsCubemap())
		{
			viewDimension = COLOR_BUFFER_VIEW_DIMENSION_TEXTURECUBEARRAY;
		}

		break;
	}

	case DirectX::TEX_DIMENSION_TEXTURE3D:
		viewDimension = COLOR_BUFFER_VIEW_DIMENSION_TEXTURE3D;
		depthOrArraySize = metaData.depth;
		
		break;

	default:
		viewDimension = COLOR_BUFFER_VIEW_DIMENSION_UNKNOWN;
		break;
	}

	mTexture = make_unique<ColorBuffer>(
		metaData.width,
		metaData.height,
		depthOrArraySize,
		viewDimension,
		metaData.format,
		texHeap,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		allocator,
		nullptr,
		nullptr,
		D3D12_RESOURCE_FLAG_NONE,
		nullptr,
		metaData.mipLevels);

	vector<D3D12_SUBRESOURCE_DATA> subresources(scratchImage.GetImageCount());
	const Image* images = scratchImage.GetImages();

	for (int i = 0; i < subresources.size(); ++i)
	{
		subresources[i].RowPitch = images[i].rowPitch;
		subresources[i].SlicePitch = images[i].slicePitch;
		subresources[i].pData = images[i].pixels;
	}

	mTexture->CopySubresources(cmdList, uploadHeap, subresources.data(), 0, subresources.size());
}

uint32_t Carol::Texture::GetGpuSrvIdx(uint32_t planeSlice)
{
	return mTexture->GetGpuSrvIdx(planeSlice);
}

	void Carol::Texture::ReleaseIntermediateBuffer()
{
	mTexture->ReleaseIntermediateBuffer();
}

uint32_t Carol::Texture::GetRef()
{
	return mNumRef;
}

void Carol::Texture::AddRef()
{
}

void Carol::Texture::DecRef()
{
}

Carol::TextureManager::TextureManager(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	Heap* texHeap,
	Heap* uploadHeap,
	DescriptorAllocator* allocator)
	:mDevice(device),
	mCommandList(cmdList),
	mTexturesHeap(texHeap),
	mUploadBuffersHeap(uploadHeap),
	mAllocator(allocator)
{
}

uint32_t Carol::TextureManager::LoadTexture(const wstring& fileName, bool isSrgb)
{
	if (fileName.size() == 0)
	{
		return -1;
	}

	if (mTextures.count(fileName) == 0)
	{
		mTextures[fileName] = make_unique<Texture>(fileName, false, mCommandList, mTexturesHeap, mUploadBuffersHeap, mAllocator);
	}
	else
	{
		mTextures[fileName]->AddRef();
	}

	return mTextures[fileName]->GetGpuSrvIdx();
}

void Carol::TextureManager::UnloadTexture(const wstring& fileName)
{
	mDeletedTextures[mCurrFrame].push_back(fileName);
}

void Carol::TextureManager::DelayedDelete(uint32_t currFrame)
{
	mCurrFrame = currFrame;

	if (mCurrFrame >= mDeletedTextures.size())
	{
		mDeletedTextures.emplace_back();
	}

	for (auto& fileName : mDeletedTextures[mCurrFrame])
	{
		mTextures[fileName]->DecRef();
		if (mTextures[fileName]->GetRef() == 0)
		{
			mTextures.erase(fileName);
		}
	}

	mDeletedTextures[mCurrFrame].clear();
}

void Carol::TextureManager::ReleaseIntermediateBuffers(const std::wstring& fileName)
{
	if (mTextures.count(fileName) == 0)
	{
		return;
	}

	mTextures[fileName]->ReleaseIntermediateBuffer();
}


