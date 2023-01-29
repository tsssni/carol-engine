export module carol.renderer.render_pass.display_pass;
import carol.renderer.render_pass.render_pass;
import carol.renderer.dx12;
import carol.renderer.utils;
import <wrl/client.h>;
import <comdef.h>;
import <dxgi1_6.h>;
import <DirectXMath.h>;
import <DirectXColors.h>;
import <vector>;
import <string>;
import <memory>;

namespace Carol
{
    using Microsoft::WRL::ComPtr;
    using std::make_unique;
    using std::unique_ptr;
    using std::vector;

    export class DisplayPass : public RenderPass
	{
	public:
		DisplayPass(
			HWND hwnd,
			IDXGIFactory* factory,
			uint32_t bufferCount,
			DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM
		)
        :mBackBuffer(bufferCount),
        mBackBufferFormat(backBufferFormat),
        mBackBufferRtvAllocInfo(make_unique<DescriptorAllocInfo>())
        {
            mBackBufferFormat = backBufferFormat;
            mBackBuffer.resize(bufferCount);

            DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
            swapChainDesc.BufferDesc.Width = 0;
            swapChainDesc.BufferDesc.Height = 0;
            swapChainDesc.BufferDesc.RefreshRate.Numerator = 144;
            swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
            swapChainDesc.BufferDesc.Format = backBufferFormat;
            swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
            swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
            swapChainDesc.SampleDesc.Count = 1;
            swapChainDesc.SampleDesc.Quality = 0;
            swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
            swapChainDesc.BufferCount = bufferCount;
            swapChainDesc.OutputWindow = hwnd;
            swapChainDesc.Windowed = true;
            swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

            mSwapChain.Reset();
            ThrowIfFailed(factory->CreateSwapChain(gCommandQueue.Get(), &swapChainDesc, mSwapChain.GetAddressOf()));
        }

		DisplayPass(const DisplayPass&) = delete;
		DisplayPass(DisplayPass&&) = delete;
		DisplayPass& operator=(const DisplayPass&) = delete;

		~DisplayPass()
        {
            gDescriptorManager->RtvDeallocate(std::move(mBackBufferRtvAllocInfo));
        }
	
		virtual IDXGISwapChain* GetSwapChain()
        {
            return mSwapChain.Get();
        }

		IDXGISwapChain** GetAddressOfSwapChain()
        {
            return mSwapChain.GetAddressOf();
        }

		void SetBackBufferIndex()
        {
            mCurrBackBufferIndex = (mCurrBackBufferIndex + 1) % mBackBuffer.size();
        }

		uint32_t GetBackBufferCount()
        {
            return mBackBuffer.size();
        }

		Resource* GetCurrBackBuffer()
        {
            return mBackBuffer[mCurrBackBufferIndex].get();
        }

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCurrBackBufferRtv()
        {
            return gDescriptorManager->GetRtvHandle(mBackBufferRtvAllocInfo.get(), mCurrBackBufferIndex);
        }

		DXGI_FORMAT GetBackBufferFormat()
        {
            return mBackBufferFormat;
        }

		virtual void Draw()override
        {

        }

		virtual void Update()override
        {

        }

		virtual void ReleaseIntermediateBuffers()override
        {

        }

		void Present()
        {
            if (FAILED(mSwapChain->Present(0, 0)))
            {
                ThrowIfFailed(gDevice->GetDeviceRemovedReason());
            }

            SetBackBufferIndex();
        }

	private:
		virtual void InitShaders()override
        {

        }

		virtual void InitPSOs()override
        {

        }

		virtual void InitBuffers()override
        {
            for (int i = 0; i < mBackBuffer.size(); ++i)
            {
                mBackBuffer[i] = make_unique<Resource>();
            }

            ThrowIfFailed(mSwapChain->ResizeBuffers(
                mBackBuffer.size(),
                mWidth,
                mHeight,
                mBackBufferFormat,
                DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
            ));

            mCurrBackBufferIndex = 0;
            InitDescriptors();
        }

		void InitDescriptors()
        {
            if (!mBackBufferRtvAllocInfo->NumDescriptors)
            {
                mBackBufferRtvAllocInfo = gDescriptorManager->RtvAllocate(mBackBuffer.size());
            }
            
            for (int i = 0; i < mBackBuffer.size(); ++i)
            {
                ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(mBackBuffer[i]->GetAddressOf())));
                gDevice->CreateRenderTargetView(mBackBuffer[i]->Get(), nullptr, gDescriptorManager->GetRtvHandle(mBackBufferRtvAllocInfo.get(), i));
            }
        }

		ComPtr<IDXGISwapChain> mSwapChain;
		uint32_t mCurrBackBufferIndex;

		vector<unique_ptr<Resource>> mBackBuffer;
		unique_ptr<DescriptorAllocInfo> mBackBufferRtvAllocInfo;
		DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	};

    export unique_ptr<DisplayPass> gDisplayPass;
}