#include "Texture.h"
#include "../Utils/Common.h"
#include <DirectXTex.h>
#include "../DirectX/d3dx12.h"
#include <memory>
#include <vector>

using DirectX::TexMetadata;
using DirectX::ScratchImage;
using DirectX::Image;
using DirectX::MakeSRGB;
using DirectX::LoadFromDDSFile;
using DirectX::LoadFromWICFile;
using std::make_unique;
using std::vector;

Carol::DefaultBuffer* Carol::Texture::GetBuffer()
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
		mTexDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		mTexDesc.Texture2D.MipLevels = texResDesc.MipLevels;
		mTexDesc.Texture2D.MostDetailedMip = 0;
		mTexDesc.Texture2D.ResourceMinLODClamp = 0.0f;
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

void Carol::Texture::LoadTexture(wstring fileName, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, bool isSrgb)
{
	mTexture = make_unique<DefaultBuffer>();

	wstring suffix = fileName.substr(fileName.find_last_of(L'.') + 1, 3);
	TexMetadata metaData;
	ScratchImage scratchImage;

	if (suffix == L"dds")
	{
		ThrowIfFailed(LoadFromDDSFile(fileName.c_str(), DirectX::DDS_FLAGS_FORCE_RGB, &metaData, scratchImage));

	}
	else
	{
		ThrowIfFailed(LoadFromWICFile(fileName.c_str(), DirectX::WIC_FLAGS_FORCE_RGB, &metaData, scratchImage));
	}

	if (isSrgb)
	{
		metaData.format = DirectX::MakeSRGB(metaData.format);
	}

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

	mTexture->InitDefaultBuffer(device, D3D12_HEAP_FLAG_NONE, &texResDesc, nullptr);

	vector<D3D12_SUBRESOURCE_DATA> subresources(scratchImage.GetImageCount());
	const Image* images = scratchImage.GetImages();

	for (int i=0;i<subresources.size();++i)
	{
		subresources[i].RowPitch = images[i].rowPitch;
		subresources[i].SlicePitch = images[i].slicePitch;
		subresources[i].pData = images[i].pixels;
	}

	mTexture->CopySubresources(cmdList, subresources.data(), 0, subresources.size());
	SetDesc();
}
