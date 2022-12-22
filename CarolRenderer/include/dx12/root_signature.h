#pragma once
#include <utils/d3dx12.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <comdef.h>
#include <vector>
#include <memory>
#include <string>

namespace Carol
{
	class DescriptorAllocator;
	class DescriptorAllocInfo;

	class RootSignature
	{
	public:
		RootSignature(ID3D12Device* device, DescriptorAllocator* allocator);
		~RootSignature();

		void ClearAllocInfo();
		ID3D12RootSignature* Get();

		uint32_t AllocateTex1D(DescriptorAllocInfo* info);
		uint32_t AllocateTex2D(DescriptorAllocInfo* info);
		uint32_t AllocateTex3D(DescriptorAllocInfo* info);
		uint32_t AllocateTexCube(DescriptorAllocInfo* info);

		void AllocateTex1DBundle();
		void AllocateTex2DBundle();
		void AllocateTex3DBundle();
		void AllocateTexCubeBundle();
		
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetTex1DBundleHandle();
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetTex2DBundleHandle();
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetTex3DBundleHandle();
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetTexCubeBundleHandle();

		enum {
			ROOT_SIGNATURE_FRAME_CB,
			ROOT_SIGNATURE_MESH_CB,
			ROOT_SIGNATURE_SKINNED_CB,
			ROOT_SIGNATURE_MANAGER_CB,
			ROOT_SIGNATURE_TEX1D,
			ROOT_SIGNATURE_TEX2D,
			ROOT_SIGNATURE_TEX3D,
			ROOT_SIGNATURE_TEXCUBE,
			ROOT_SIGNATURE_OITPPLL_UAV,
			ROOT_SIGNATURE_OITPPLL_SRV,
			ROOT_SIGNATURE_COUNT
		};
	protected:
		enum {
			RANGE_TEX1D,
			RANGE_TEX2D,
			RANGE_TEX3D,
			RANGE_TEXCUBE,
			RANGE_OITPPLL_UAV,
			RANGE_OITPPLL_SRV,
			RANGE_COUNT
		};

		uint32_t CountNumDescriptors(std::vector<DescriptorAllocInfo*>& infos);
		void CopyDescriptors(DescriptorAllocInfo* texInfo, std::vector<DescriptorAllocInfo*>& infos);

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
		ID3D12Device* mDevice;
		DescriptorAllocator* mAllocator;

		std::unique_ptr<DescriptorAllocInfo> mTex1DAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mTex2DAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mTex3DAllocInfo;
		std::unique_ptr<DescriptorAllocInfo> mTexCubeAllocInfo;

		std::vector<DescriptorAllocInfo*> mTex1DInfos;
		std::vector<DescriptorAllocInfo*> mTex2DInfos;
		std::vector<DescriptorAllocInfo*> mTex3DInfos;
		std::vector<DescriptorAllocInfo*> mTexCubeInfos;

		uint32_t mTex1DCount;
		uint32_t mTex2DCount;
		uint32_t mTex3DCount;
		uint32_t mTexCubeCount;
	};
}
