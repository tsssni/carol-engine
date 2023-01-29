#pragma once
#include <utils/d3dx12.h>
#include <wrl/client.h>
#include <memory>
#include <unordered_map>
#include <string>
#include <string_view>

namespace Carol
{
	class ColorBuffer;
	
	class Texture
	{
	public:
		Texture(std::wstring_view fileName, bool isSrgb);

		uint32_t GetGpuSrvIdx(uint32_t planeSlice = 0);
		void ReleaseIntermediateBuffer();

		uint32_t GetRef();
		void AddRef();
		void DecRef();
	
		std::unique_ptr<ColorBuffer> mTexture;
		uint32_t mNumRef;
	};

	class TextureManager
	{
	public:
		TextureManager();

		uint32_t LoadTexture(std::wstring_view fileName, bool isSrgb = false);
		void UnloadTexture(std::wstring_view fileName);
		void ReleaseIntermediateBuffers(std::wstring_view fileName);

	protected:
		std::unordered_map<std::wstring, std::unique_ptr<Texture>> mTextures;
	};
}


