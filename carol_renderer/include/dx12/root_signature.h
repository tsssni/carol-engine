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
	enum RootParameter {
			MESH_CB,
			SKINNED_CB,
			PASS_CONSTANTS,
			FRAME_CB,
			ROOT_SIGNATURE_COUNT
		};

	class RootSignature
	{
	public:
		RootSignature();
		ID3D12RootSignature* Get()const;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
	};
}
