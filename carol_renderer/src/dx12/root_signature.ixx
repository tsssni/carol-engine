export module carol.renderer.dx12.root_signature;
import carol.renderer.dx12.command;
import carol.renderer.dx12.shader;
import carol.renderer.utils;
import <wrl/client.h>;
import <comdef.h>;
import <vector>;
import <memory>;
import <string>;

namespace Carol
{
	using Microsoft::WRL::ComPtr;
	using std::unique_ptr;

	export enum RootParameter {
			MESH_CB,
			SKINNED_CB,
			PASS_CONSTANTS,
			FRAME_CB,
			ROOT_SIGNATURE_COUNT
		};

	export class RootSignature
	{
	public:
		RootSignature(ID3D12Device* device)
        {
            Shader rootSignatureShader(L"shader\\root_signature.hlsl", {}, L"main", L"ms_6_6");
            ThrowIfFailed(device->CreateRootSignature(0, rootSignatureShader.GetBufferPointer(), rootSignatureShader.GetBufferSize(), IID_PPV_ARGS(mRootSignature.GetAddressOf())));
        }

		ID3D12RootSignature* Get()const
        {
            return mRootSignature.Get();
        }

		ComPtr<ID3D12RootSignature> mRootSignature;
	};

	export unique_ptr<RootSignature> gRootSignature;
}