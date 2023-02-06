export module carol.renderer.render_pass.render_pass;
import carol.renderer.utils;
import <wrl/client.h>;
import <memory>;

namespace Carol
{
	export class RenderPass
	{
	public:
		virtual void Draw() = 0;
		virtual void Update() = 0;
		virtual void OnResize(uint32_t width, uint32_t height)
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

	protected:
		virtual void InitShaders() = 0;
		virtual void InitPSOs() = 0;
		virtual void InitBuffers() = 0;

		D3D12_VIEWPORT mViewport;
		D3D12_RECT mScissorRect;
		uint32_t mWidth;
		uint32_t mHeight;
	};
}