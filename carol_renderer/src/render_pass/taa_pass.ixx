export module carol.renderer.render_pass.taa_pass;
import carol.renderer.render_pass.render_pass;
import carol.renderer.render_pass.display_pass;
import carol.renderer.render_pass.frame_pass;
import carol.renderer.dx12;
import carol.renderer.scene;
import carol.renderer.utils;
import <DirectXMath.h>;
import <DirectXColors.h>;
import <memory>;
import <cmath>;
import <string_view>;

namespace Carol
{
    using std::make_unique;
    using std::unique_ptr;
    using std::vector;
    using std::wstring;
    using std::wstring_view;
    using namespace DirectX;

    export class TaaPass : public RenderPass
	{
	public:
		TaaPass(
			DXGI_FORMAT velocityMapFormat = DXGI_FORMAT_R16G16_FLOAT,
			DXGI_FORMAT frameFormat = DXGI_FORMAT_R8G8B8A8_UNORM)
            :mVelocityMapFormat(velocityMapFormat),
            mFrameFormat(frameFormat)
        {
            InitHalton();
            InitShaders();
            InitPSOs();
        }

		TaaPass(const TaaPass&) = delete;
		TaaPass(TaaPass&&) = delete;
		TaaPass& operator=(const TaaPass&) = delete;

		virtual void Draw()override
        {
            DrawVelocityMap();
            DrawOutput();
        }

		virtual void Update()override
        {

        }

		virtual void ReleaseIntermediateBuffers()override
        {

        }

		void GetHalton(float& proj0,float& proj1)
        {
            static int i = 0;

            proj0 = (2 * mHalton[i].x - 1) / mWidth;
            proj1 = (2 * mHalton[i].y - 1) / mHeight;

            i = (i + 1) % 8;
        }

		void SetHistViewProj(XMMATRIX& histViewProj)
        {
            mHistViewProj = histViewProj;
        }

		XMMATRIX GetHistViewProj()
        {
            return mHistViewProj;
        }
		
		uint32_t GetVeloctiySrvIdx()
        {
            return mVelocityMap->GetGpuSrvIdx();
        }

		uint32_t GetHistFrameSrvIdx()
        {
            return mHistFrameMap->GetGpuSrvIdx();
        }

	protected:
		virtual void InitShaders()override
        {
            vector<wstring_view> nullDefines{};

            vector<wstring_view> skinnedDefines =
            {
                L"SKINNED=1"
            };

            gShaders[L"TaaVelocityStaticMS"] = make_unique<Shader>(L"shader\\velocity_ms.hlsl", nullDefines, L"main", L"ms_6_6");
            gShaders[L"TaaVelocityPS"] = make_unique<Shader>(L"shader\\velocity_ps.hlsl", nullDefines, L"main", L"ps_6_6");
            gShaders[L"TaaVelocitySkinnedMS"] = make_unique<Shader>(L"shader\\velocity_ms.hlsl", skinnedDefines, L"main", L"ms_6_6");
            gShaders[L"TaaOutputPS"] = make_unique<Shader>(L"shader\\taa_ps.hlsl", nullDefines, L"main", L"ps_6_6");
        }

		virtual void InitPSOs()override
        {
            auto velocityStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
            velocityStaticMeshPSO->SetRenderTargetFormat(mVelocityMapFormat, gFramePass->GetFrameDsvFormat());
            velocityStaticMeshPSO->SetAS(gShaders[L"CullAS"].get());
            velocityStaticMeshPSO->SetMS(gShaders[L"TaaVelocityStaticMS"].get());
            velocityStaticMeshPSO->SetPS(gShaders[L"TaaVelocityPS"].get());
            velocityStaticMeshPSO->Finalize();
            gPSOs[L"VelocityStatic"] = std::move(velocityStaticMeshPSO);

            auto velocitySkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
            velocitySkinnedMeshPSO->SetRenderTargetFormat(mVelocityMapFormat, gFramePass->GetFrameDsvFormat());
            velocitySkinnedMeshPSO->SetAS(gShaders[L"CullAS"].get());
            velocitySkinnedMeshPSO->SetMS(gShaders[L"TaaVelocitySkinnedMS"].get());
            velocitySkinnedMeshPSO->SetPS(gShaders[L"TaaVelocityPS"].get());
            velocitySkinnedMeshPSO->Finalize();
            gPSOs[L"VelocitySkinned"] = std::move(velocitySkinnedMeshPSO);

            auto outputMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
            outputMeshPSO->SetDepthStencilState(gDepthDisabledState);
            outputMeshPSO->SetRenderTargetFormat(gFramePass->GetFrameRtvFormat());
            outputMeshPSO->SetMS(gShaders[L"ScreenMS"].get());
            outputMeshPSO->SetPS(gShaders[L"TaaOutputPS"].get());
            outputMeshPSO->Finalize();
            gPSOs[L"TaaOutput"] = std::move(outputMeshPSO);
        }

		virtual void InitBuffers()override
        {
            mHistFrameMap = make_unique<ColorBuffer>(
                mWidth,
                mHeight,
                1,
                COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
                mFrameFormat,
                gHeapManager->GetDefaultBuffersHeap(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            
            D3D12_CLEAR_VALUE optClearValue = CD3DX12_CLEAR_VALUE(mVelocityMapFormat, Colors::Black);
            mVelocityMap = make_unique<ColorBuffer>(
                mWidth,
                mHeight,
                1,
                COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
                mVelocityMapFormat,
                gHeapManager->GetDefaultBuffersHeap(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
                &optClearValue);
        }

		void InitHalton()
        {
            for (int i = 0; i < 8; ++i)
            {
                mHalton[i].x = RadicalInversion(2, i + 1);
                mHalton[i].y = RadicalInversion(3, i + 1);
            }
        }

		float RadicalInversion(int base, int num)
        {
            int temp[4];
            int i = 0;

            while (num != 0)
            {
                temp[i++] = num % base;
                num /= base;
            }

            double convertionResult = 0;
            for (int j = 0; j < i; ++j)
            {
                convertionResult += temp[j] * pow(base, -j - 1);
            }

            return convertionResult;
        }

        void DrawVelocityMap()
        {
            gCommandList->RSSetViewports(1, &mViewport);
            gCommandList->RSSetScissorRects(1, &mScissorRect);

            gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mVelocityMap->Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)));
            gCommandList->ClearRenderTargetView(mVelocityMap->GetRtv(), Colors::Black, 0, nullptr);
            gCommandList->ClearDepthStencilView(gFramePass->GetFrameDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);
            gCommandList->OMSetRenderTargets(1, GetRvaluePtr(mVelocityMap->GetRtv()), true, GetRvaluePtr(gFramePass->GetFrameDsv()));

            gCommandList->SetPipelineState(gPSOs[L"VelocityStatic"]->Get());
            gScene->ExecuteIndirect(gFramePass->GetIndirectCommandBuffer(OPAQUE_STATIC));

            gCommandList->SetPipelineState(gPSOs[L"VelocitySkinned"]->Get());
            gScene->ExecuteIndirect(gFramePass->GetIndirectCommandBuffer(OPAQUE_SKINNED));

            gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mVelocityMap->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));
        }

        void DrawOutput()
        {
            gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(gDisplayPass->GetCurrBackBuffer()->Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)));
            gCommandList->ClearRenderTargetView(gDisplayPass->GetCurrBackBufferRtv(), Colors::Gray, 0, nullptr);
            gCommandList->OMSetRenderTargets(1, GetRvaluePtr(gDisplayPass->GetCurrBackBufferRtv()), true, nullptr);
            gCommandList->SetPipelineState(gPSOs[L"TaaOutput"]->Get());
            static_cast<ID3D12GraphicsCommandList6*>(gCommandList.Get())->DispatchMesh(1, 1, 1);

            gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mHistFrameMap->Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST)));
            gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(gDisplayPass->GetCurrBackBuffer()->Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE)));
            gCommandList->CopyResource(mHistFrameMap->Get(), gDisplayPass->GetCurrBackBuffer()->Get());
            gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mHistFrameMap->Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));
            gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(gDisplayPass->GetCurrBackBuffer()->Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT)));
        }

		unique_ptr<ColorBuffer> mHistFrameMap;
		unique_ptr<ColorBuffer> mVelocityMap;

		DXGI_FORMAT mVelocityMapFormat = DXGI_FORMAT_R16G16_SNORM;
		DXGI_FORMAT mFrameFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

		XMFLOAT2 mHalton[8];
		XMMATRIX mHistViewProj;
	};
}