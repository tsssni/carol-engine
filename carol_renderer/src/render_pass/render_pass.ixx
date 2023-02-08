export module carol.renderer.render_pass.render_pass;
import carol.renderer.dx12;
import carol.renderer.utils;
import <wrl/client.h>;
import <memory>;

namespace Carol
{
	using Microsoft::WRL::ComPtr;
	using std::make_unique;
	using std::unique_ptr;

	export class RenderPass
	{
	public:
		virtual void Draw(ID3D12GraphicsCommandList* cmdList) = 0;

		virtual void OnResize(
			uint32_t width,
			uint32_t height,
			ID3D12Device* device,
			Heap* heap,
			DescriptorManager* descriptorManager)
		{
			if (mWidth != width || mHeight != height)
			{
				mWidth = width;
				mHeight = height;
				mViewport = { 0.f,0.f,mWidth * 1.f,mHeight * 1.f,0.f,1.f };
				mScissorRect = { 0,0,(long)mWidth,(long)mHeight };

				InitBuffers(device, heap, descriptorManager);
			}
		}

		static void Init(ID3D12Device* device)
		{
			Shader::InitCompiler();
			sRootSignature = make_unique<RootSignature>(device);

			D3D12_INDIRECT_ARGUMENT_DESC argDesc[3];

			argDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
			argDesc[0].ConstantBufferView.RootParameterIndex = MESH_CB;
			
			argDesc[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
			argDesc[1].ConstantBufferView.RootParameterIndex = SKINNED_CB;

			argDesc[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;
			
			D3D12_COMMAND_SIGNATURE_DESC cmdSigDesc;
			cmdSigDesc.pArgumentDescs = argDesc;
			cmdSigDesc.NumArgumentDescs = _countof(argDesc);
			cmdSigDesc.ByteStride = sizeof(IndirectCommand);
			cmdSigDesc.NodeMask = 0;

			ThrowIfFailed(device->CreateCommandSignature(&cmdSigDesc, sRootSignature->Get(), IID_PPV_ARGS(sCommandSignature.GetAddressOf())));
		}

		static const RootSignature* GetRootSignature()
		{
			return sRootSignature.get();
		}
	protected:
		virtual void InitShaders() = 0;
		virtual void InitPSOs(ID3D12Device* device) = 0;
		virtual void InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager) = 0;

		void ExecuteIndirect(ID3D12GraphicsCommandList* cmdList, const StructuredBuffer* indirectCmdBuffer)
		{
			cmdList->ExecuteIndirect(
				sCommandSignature.Get(),
				indirectCmdBuffer->GetNumElements(),
				indirectCmdBuffer->Get(),
				0,
				indirectCmdBuffer->Get(),
				indirectCmdBuffer->GetCounterOffset());
		}

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;
		uint32_t mWidth;
		uint32_t mHeight;

		static ComPtr<ID3D12CommandSignature> sCommandSignature;
		static unique_ptr<RootSignature> sRootSignature;
	};

	ComPtr<ID3D12CommandSignature> RenderPass::sCommandSignature = nullptr;
	unique_ptr<RootSignature> RenderPass::sRootSignature = nullptr;
}