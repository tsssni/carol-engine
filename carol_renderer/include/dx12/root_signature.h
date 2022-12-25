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
		ID3D12RootSignature* Get();

		enum {
			ROOT_SIGNATURE_FRAME_CB,
			ROOT_SIGNATURE_MESH_CB,
			ROOT_SIGNATURE_SKINNED_CB,
			ROOT_SIGNATURE_SSAO_CB,
			ROOT_SIGNATURE_CONSTANT,
			ROOT_SIGNATURE_OITPPLL_UAV,
			ROOT_SIGNATURE_SRV_0,
			ROOT_SIGNATURE_SRV_1,
			ROOT_SIGNATURE_SRV_2,
			ROOT_SIGNATURE_SRV_3,
			ROOT_SIGNATURE_TEX1D,
			ROOT_SIGNATURE_TEX2D,
			ROOT_SIGNATURE_TEX3D,
			ROOT_SIGNATURE_TEXCUBE,
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

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
	};
}
