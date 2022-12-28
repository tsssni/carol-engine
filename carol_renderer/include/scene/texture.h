#pragma once
#include <d3d12.h>
#include <utils/d3dx12.h>
#include <wrl/client.h>
#include <memory>
#include <unordered_map>
#include <string>

namespace Carol
{
	class DefaultResource;
	class Heap;
	class CbvSrvUavDescriptorAllocator;
	class DescriptorAllocInfo;
	class RootSignature;
	
	class Texture
	{
	public:
		DefaultResource* GetResource();
		D3D12_SHADER_RESOURCE_VIEW_DESC GetDesc();
		void LoadTexture(ID3D12GraphicsCommandList* cmdList, Heap* texHeap, Heap* uploadHeap, std::wstring fileName, bool isSrgb = false);
		void ReleaseIntermediateBuffer();
	
		void SetDesc();
		std::unique_ptr<DefaultResource> mTexture;
		D3D12_SHADER_RESOURCE_VIEW_DESC mTexDesc;

		bool mIsCube = false;
		bool mIsVolume = false;
	};

	class AllocatedTexture
	{
	public:
		AllocatedTexture();
		std::unique_ptr<Texture> Texture;
		std::unique_ptr<DescriptorAllocInfo> CpuAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> GpuAllocInfo;
		uint32_t NumRef;
	};

	class TextureManager
	{
	public:
		TextureManager(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, Heap* texHeap, Heap* uploadHeap, CbvSrvUavDescriptorAllocator* allocator);

		uint32_t LoadTexture(const std::wstring& fileName, bool isSrgb = false);
		void UnloadTexture(const std::wstring& fileName);
		void ReleaseIntermediateBuffers(const std::wstring& fileName);

		void DelayedDelete(uint32_t currFrame);

	protected:

		Heap* mTexturesHeap;
		Heap* mUploadBuffersHeap;

		ID3D12Device* mDevice;
		ID3D12GraphicsCommandList* mCommandList;
		CbvSrvUavDescriptorAllocator* mDescriptorAllocator;
		RootSignature* mRootSignature;
		
		std::unordered_map<std::wstring, std::unique_ptr<AllocatedTexture>> mTextures;
		std::vector<std::vector<std::wstring>> mDeletedTextures;
		uint32_t mCurrFrame;
	};
}


