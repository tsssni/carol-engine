#pragma once
#include <d3d12.h>
#include <utils/d3dx12.h>
#include <wrl/client.h>
#include <memory>
#include <unordered_map>
#include <string>

namespace Carol
{
	class ColorBuffer;
	class HeapManager;
	class DescriptorManager;
	
	class Texture
	{
	public:
		Texture(std::wstring fileName, bool isSrgb, ID3D12GraphicsCommandList* cmdList, HeapManager* heapManager, DescriptorManager* allocator);

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
		TextureManager(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, HeapManager* heapManager, DescriptorManager* allocator);

		uint32_t LoadTexture(const std::wstring& fileName, bool isSrgb = false);
		void UnloadTexture(const std::wstring& fileName);
		void ReleaseIntermediateBuffers(const std::wstring& fileName);

		void DelayedDelete(uint32_t currFrame);

	protected:
		ID3D12Device* mDevice;
		ID3D12GraphicsCommandList* mCommandList;
		HeapManager* mHeapManager;
		DescriptorManager* mDescriptorManager;

		std::unordered_map<std::wstring, std::unique_ptr<Texture>> mTextures;
		std::vector<std::vector<std::wstring>> mDeletedTextures;
		uint32_t mCurrFrame;
	};
}


