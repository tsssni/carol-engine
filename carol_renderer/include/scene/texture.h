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
		std::unique_ptr<Texture> Texture;
		std::unique_ptr<DescriptorAllocInfo> CpuDescriptorAllocInfo;
		uint32_t NumRef = 1;
	};

	class TextureManager
	{
	public:
		TextureManager(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, Heap* texHeap, Heap* uploadHeap, CbvSrvUavDescriptorAllocator* allocator);

		void LoadTexture(const std::wstring& fileName, bool isSrgb = false);
		void UnloadTexture(const std::wstring& fileName);
		void ReleaseIntermediateBuffers(const std::wstring& fileName);

		void ClearGpuTextures();
		uint32_t CollectGpuTextures(const std::wstring& fileName);
	protected:

		Heap* mTexturesHeap;
		Heap* mUploadBuffersHeap;

		ID3D12Device* mDevice;
		ID3D12GraphicsCommandList* mCommandList;
		CbvSrvUavDescriptorAllocator* mDescriptorAllocator;
		RootSignature* mRootSignature;
		
		std::unordered_map<std::wstring, std::unique_ptr<AllocatedTexture>> mTextures;
		std::unordered_map<std::wstring, uint32_t> mGpuTexIdx;
	};
}


