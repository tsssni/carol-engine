#include <render_pass/render_pass.h>
#include <dx12/resource.h>
#include <dx12/shader.h>
#include <dx12/root_signature.h>
#include <dx12/indirect_command.h>
#include <utils/common.h>
#include <global.h>

namespace Carol
{
	using std::unique_ptr;
	using std::make_unique;
	using Microsoft::WRL::ComPtr;
}

void Carol::RenderPass::OnResize(
	uint32_t width,
	uint32_t height)
{
	if (mWidth != width || mHeight != height)
	{
		mWidth = width;
		mHeight = height;
		mMipLevel = std::max(ceilf(log2f(mWidth)), ceilf(log2f(mHeight)));
		mViewport = { 0.f,0.f,mWidth * 1.f,mHeight * 1.f,0.f,1.f };
		mScissorRect = { 0,0,(long)mWidth,(long)mHeight };

		InitBuffers();
	}
}

void Carol::RenderPass::Init()
{
	Shader::InitCompiler();
	Shader::InitShaders();
	gRootSignature = make_unique<RootSignature>();

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

	ThrowIfFailed(gDevice->CreateCommandSignature(&cmdSigDesc, gRootSignature->Get(), IID_PPV_ARGS(gCommandSignature.GetAddressOf())));
}

void Carol::RenderPass::ExecuteIndirect(const StructuredBuffer* indirectCmdBuffer)
{
	if (indirectCmdBuffer && indirectCmdBuffer->GetNumElements() > 0)
	{
		gGraphicsCommandList->ExecuteIndirect(
			gCommandSignature.Get(),
			indirectCmdBuffer->GetNumElements(),
			indirectCmdBuffer->Get(),
			0,
			indirectCmdBuffer->Get(),
			indirectCmdBuffer->GetCounterOffset());
	}
}
