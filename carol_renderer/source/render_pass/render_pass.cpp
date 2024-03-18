#include <render_pass/render_pass.h>
#include <dx12/resource.h>
#include <dx12/shader.h>
#include <dx12/root_signature.h>
#include <dx12/indirect_command.h>
#include <utils/exception.h>
#include <global.h>

void Carol::RenderPass::OnResize(
	uint32_t width,
	uint32_t height)
{
	if (mWidth != width || mHeight != height)
	{
		mWidth = width;
		mHeight = height;
		mMipLevel = std::fmax(ceilf(log2f(mWidth)), ceilf(log2f(mHeight)));
		mViewport = { 0.f,0.f,mWidth * 1.f,mHeight * 1.f,0.f,1.f };
		mScissorRect = { 0,0,(long)mWidth,(long)mHeight };

		InitBuffers();
	}
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
