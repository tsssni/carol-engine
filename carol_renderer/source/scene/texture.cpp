#include <scene/texture.h>
#include <dx12/heap.h>
#include <dx12/resource.h>
#include <utils/exception.h>
#include <global.h>
#include <DirectXTex.h>
#include <Windows.h>
#include <memory>
#include <vector>

std::wstring StringToWString(std::string_view str)
{
    WCHAR buffer[512];
	MultiByteToWideChar(CP_ACP, 0, str.data(), -1, buffer, 512);
	return std::wstring(buffer);
}

Carol::Texture::Texture(
	std::string_view fileName,
	bool isSrgb)
	:mNumRef(1)
{
	std::string_view suffix = fileName.substr(fileName.find_last_of(L'.') + 1, 3);
	DirectX::TexMetadata metaData;
	DirectX::ScratchImage scratchImage;

	if (suffix == "dds")
	{
		ThrowIfFailed(LoadFromDDSFile(StringToWString(fileName).data(), DirectX::DDS_FLAGS_NONE, &metaData, scratchImage));
	}
	else if (suffix == "tga")
	{
		ThrowIfFailed(LoadFromTGAFile(StringToWString(fileName).data(), DirectX::TGA_FLAGS_NONE, &metaData, scratchImage));
	}
	else
	{
		ThrowIfFailed(LoadFromWICFile(StringToWString(fileName).data(), DirectX::WIC_FLAGS_NONE, &metaData, scratchImage));
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

	mTexture = std::make_unique<ColorBuffer>(
		metaData.width,
		metaData.height,
		depthOrArraySize,
		viewDimension,
		metaData.format,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_NONE,
		nullptr,
		metaData.mipLevels);

	std::vector<D3D12_SUBRESOURCE_DATA> subresources(scratchImage.GetImageCount());
	const DirectX::Image* images = scratchImage.GetImages();

	for (int i = 0; i < subresources.size(); ++i)
	{
		subresources[i].RowPitch = images[i].rowPitch;
		subresources[i].SlicePitch = images[i].slicePitch;
		subresources[i].pData = images[i].pixels;
	}

	mTexture->CopySubresources(gHeapManager->GetUploadBuffersHeap(), subresources.data(), 0, subresources.size());
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
	++mNumRef;
}

void Carol::Texture::DecRef()
{
	--mNumRef;
}

Carol::TextureManager::TextureManager()
{
}

uint32_t Carol::TextureManager::LoadTexture(
	std::string_view fileName,
	bool isSrgb)
{
	if (fileName.size() == 0)
	{
		return -1;
	}

	std::string name(fileName);

	if (mTextures.count(name) == 0)
	{
		mTextures[name] = make_unique<Texture>(
			name,
			isSrgb);
	}
	else
	{
		mTextures[name]->AddRef();
	}

	return mTextures[name]->GetGpuSrvIdx();
}

void Carol::TextureManager::UnloadTexture(std::string_view fileName)
{
	std::string name(fileName);
	mTextures[name]->DecRef();

	if (mTextures[name]->GetRef() == 0)
	{
		mTextures.erase(name);
	}
}

void Carol::TextureManager::ReleaseIntermediateBuffers(std::string_view fileName)
{
	std::string name(fileName);

	if (mTextures.count(name) == 0)
	{
		mTextures[name]->ReleaseIntermediateBuffer();
	}
}


