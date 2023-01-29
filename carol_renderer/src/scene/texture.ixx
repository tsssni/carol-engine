export module carol.renderer.scene.texture;
import carol.renderer.dx12;
import carol.renderer.utils;
import <DirectXTex.h>;
import <wrl/client.h>;
import <memory>;
import <unordered_map>;
import <string>;
import <string_view>;
import <vector>;

namespace Carol
{
    using std::make_unique;
    using std::unique_ptr;
    using std::unordered_map;
    using std::vector;
    using std::wstring;
    using std::wstring_view;
    using namespace DirectX;

    class Texture
    {
    public:
        Texture(wstring_view fileName, bool isSrgb)
        	:mNumRef(1)
        {
            wstring_view suffix = fileName.substr(fileName.find_last_of(L'.') + 1, 3);
            TexMetadata metaData;
            ScratchImage scratchImage;

            if (suffix == L"dds")
            {
                ThrowIfFailed(LoadFromDDSFile(fileName.data(), DDS_FLAGS_FORCE_RGB, &metaData, scratchImage));
            }
            else if (suffix == L"tga")
            {
                ThrowIfFailed(LoadFromTGAFile(fileName.data(), TGA_FLAGS_FORCE_LINEAR, &metaData, scratchImage));
            }
            else
            {
                ThrowIfFailed(LoadFromWICFile(fileName.data(), WIC_FLAGS_FORCE_RGB, &metaData, scratchImage));
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
                gHeapManager->GetTexturesHeap(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
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

            mTexture->CopySubresources(gHeapManager->GetUploadBuffersHeap(), subresources.data(), 0, subresources.size(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        uint32_t GetGpuSrvIdx(uint32_t planeSlice = 0)
        {
            return mTexture->GetGpuSrvIdx(planeSlice);
        }

        void ReleaseIntermediateBuffer()
        {
            mTexture->ReleaseIntermediateBuffer();
        }

        uint32_t GetRef()
        {
            return mNumRef;
        }

        void AddRef()
        {
            ++mNumRef;
        }

        void DecRef()
        {
            --mNumRef;
        }

        unique_ptr<ColorBuffer> mTexture;
        uint32_t mNumRef;
    };

    export class TextureManager
    {
    public:
        TextureManager()
        {
        }

        uint32_t LoadTexture(wstring_view fileName, bool isSrgb = false)
        {
            if (fileName.size() == 0)
            {
                return -1;
            }

            wstring name(fileName);

            if (mTextures.count(name) == 0)
            {
                mTextures[name] = make_unique<Texture>(name, false);
            }
            else
            {
                mTextures[name]->AddRef();
            }

            return mTextures[name]->GetGpuSrvIdx();
        }

        void UnloadTexture(wstring_view fileName)
        {
            wstring name(fileName);
            mTextures[name]->DecRef();

            if (mTextures[name]->GetRef() == 0)
            {
                mTextures.erase(name);
            }
        }

        void ReleaseIntermediateBuffers(wstring_view fileName)
        {
            wstring name(fileName);

            if (mTextures.count(name))
            {
                mTextures[name]->ReleaseIntermediateBuffer();
            }
        }

    protected:
        unordered_map<wstring, unique_ptr<Texture>> mTextures;
    };

    export unique_ptr<TextureManager> gTextureManager;
}