#include <render_pass/render_pass.h>

void Carol::RenderPass::OnResize(uint32_t width, uint32_t height)
{
	if (mWidth != width || mHeight != height)
	{
		mWidth = width;
		mHeight = height;
		mViewport = { 0.f,0.f,mWidth * 1.f,mHeight * 1.f,0.f,1.f };
		mScissorRect = { 0,0,(long)mWidth,(long)mHeight };

		InitBuffers();
	}
}
