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
	class Heap;
	class DescriptorManager;
	
	class Texture
	{
	public:
		Texture(
			std::string_view fileName,
			bool isSrgb);

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

		uint32_t LoadTexture(
			std::string_view fileName,
			bool isSrgb);
		void UnloadTexture(std::string_view fileName);
		void ReleaseIntermediateBuffers(std::string_view fileName);

	protected:
		std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	};
}


