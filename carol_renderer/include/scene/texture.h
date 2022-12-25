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
	class DescriptorAllocator;
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
		TextureManager(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, Heap* texHeap, Heap* uploadHeap, RootSignature* rootSignature, DescriptorAllocator* allocator, uint32_t numFrames);
		~TextureManager();

		void LoadTexture(const std::wstring& fileName, bool isSrgb = false);
		void UnloadTexture(const std::wstring& fileName);
		void ReleaseIntermediateBuffers(const std::wstring& fileName);

		void ClearGpuTextures(uint32_t currFrame);
		uint32_t CollectGpuTextures(const std::wstring& fileName);
		void AllocateGpuTextures(uint32_t currFrame);
		
		uint32_t GetNumTex1D();
		uint32_t GetNumTex2D();
		uint32_t GetNumTex3D();
		uint32_t GetNumTexCube();

		CD3DX12_GPU_DESCRIPTOR_HANDLE GetTex1DHandle(uint32_t currFrame);
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetTex2DHandle(uint32_t currFrame);
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetTex3DHandle(uint32_t currFrame);
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetTexCubeHandle(uint32_t currFrame);
	protected:
		void CopyDescriptors(std::vector<std::wstring>& tex, DescriptorAllocInfo* info);

		Heap* mTexturesHeap;
		Heap* mUploadBuffersHeap;

		ID3D12Device* mDevice;
		ID3D12GraphicsCommandList* mCommandList;
		DescriptorAllocator* mDescriptorAllocator;
		RootSignature* mRootSignature;
		
		std::unordered_map<std::wstring, std::unique_ptr<AllocatedTexture>> mTextures;
		std::unordered_map<std::wstring, uint32_t> mGpuTexIdx;

		uint32_t mGpuTex1DCount;
		uint32_t mGpuTex2DCount;
		uint32_t mGpuTex3DCount;
		uint32_t mGpuTexCubeCount;

		std::vector<std::unique_ptr<DescriptorAllocInfo>> mGpuTex1DAllocInfo;
		std::vector<std::unique_ptr<DescriptorAllocInfo>> mGpuTex2DAllocInfo;
		std::vector<std::unique_ptr<DescriptorAllocInfo>> mGpuTex3DAllocInfo;
		std::vector<std::unique_ptr<DescriptorAllocInfo>> mGpuTexCubeAllocInfo;

		std::vector<std::wstring> mGpuTex1D;
		std::vector<std::wstring> mGpuTex2D;
		std::vector<std::wstring> mGpuTex3D;
		std::vector<std::wstring> mGpuTexCube;
	};
}


