export module carol.renderer.render_pass.taa_pass;
import carol.renderer.render_pass.render_pass;
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
            ID3D12Device* device,
            DXGI_FORMAT frameFormat,
            DXGI_FORMAT frameDsvFormat,
            DXGI_FORMAT velocityMapFormat = DXGI_FORMAT_R16G16_FLOAT)
            :mFrameFormat(frameFormat),
            mFrameDsvFormat(frameDsvFormat),
            mVelocityMapFormat(velocityMapFormat),
            mIndirectCommandBuffer(MESH_TYPE_COUNT)
        {
            InitHalton();
            InitShaders();
            InitPSOs(device);
        }

		TaaPass(const TaaPass&) = delete;
		TaaPass(TaaPass&&) = delete;
		TaaPass& operator=(const TaaPass&) = delete;

		virtual void Draw(ID3D12GraphicsCommandList* cmdList)override
        {
            DrawVelocityMap(cmdList);
            DrawOutput(cmdList);
        }

        void SetCurrBackBuffer(Resource* currBackBuffer)
        {
            mCurrBackBuffer = currBackBuffer;
        }

		void SetIndirectCommandBuffer(MeshType type, const StructuredBuffer* indirectCommandBuffer)
        {
            mIndirectCommandBuffer[type] = indirectCommandBuffer;
        }

		void SetFrameDsv(D3D12_CPU_DESCRIPTOR_HANDLE frameDsv)
        {
            mFrameDsv = frameDsv;
        }

		void SetCurrBackBufferRtv(D3D12_CPU_DESCRIPTOR_HANDLE currBackBufferRtv)
        {
            mCurrBackBufferRtv = currBackBufferRtv;
        }

		void GetHalton(float& proj0,float& proj1)const
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

		XMMATRIX GetHistViewProj()const
        {
            return mHistViewProj;
        }
		
		uint32_t GetVeloctiySrvIdx()const
        {
            return mVelocityMap->GetGpuSrvIdx();
        }

		uint32_t GetHistFrameSrvIdx()const
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

            if (gShaders.count(L"TaaVelocityStaticMS") == 0)
            {
                gShaders[L"TaaVelocityStaticMS"] = make_unique<Shader>(L"shader\\velocity_ms.hlsl", nullDefines, L"main", L"ms_6_6");
            }

            if (gShaders.count(L"TaaVelocityPS") == 0)
            {
                gShaders[L"TaaVelocityPS"] = make_unique<Shader>(L"shader\\velocity_ps.hlsl", nullDefines, L"main", L"ps_6_6");
            }

            if (gShaders.count(L"TaaVelocitySkinnedMS") == 0)
            {
                gShaders[L"TaaVelocitySkinnedMS"] = make_unique<Shader>(L"shader\\velocity_ms.hlsl", skinnedDefines, L"main", L"ms_6_6");
            }

            if (gShaders.count(L"TaaOutputPS") == 0)
            {
                gShaders[L"TaaOutputPS"] = make_unique<Shader>(L"shader\\taa_ps.hlsl", nullDefines, L"main", L"ps_6_6");
            }
        }

        virtual void InitPSOs(ID3D12Device* device)override
        {
            if (gPSOs.count(L"VelocityStatic") == 0)
            {
                auto velocityStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
                velocityStaticMeshPSO->SetRootSignature(sRootSignature.get());
                velocityStaticMeshPSO->SetRenderTargetFormat(mVelocityMapFormat, mFrameDsvFormat);
                velocityStaticMeshPSO->SetAS(gShaders[L"CullAS"].get());
                velocityStaticMeshPSO->SetMS(gShaders[L"TaaVelocityStaticMS"].get());
                velocityStaticMeshPSO->SetPS(gShaders[L"TaaVelocityPS"].get());
                velocityStaticMeshPSO->Finalize(device);
            
                gPSOs[L"VelocityStatic"] = std::move(velocityStaticMeshPSO);
            }

            if (gPSOs.count(L"VelocitySkinned") == 0)
            {
                auto velocitySkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
                velocitySkinnedMeshPSO->SetRootSignature(sRootSignature.get());
                velocitySkinnedMeshPSO->SetRenderTargetFormat(mVelocityMapFormat, mFrameDsvFormat);
                velocitySkinnedMeshPSO->SetAS(gShaders[L"CullAS"].get());
                velocitySkinnedMeshPSO->SetMS(gShaders[L"TaaVelocitySkinnedMS"].get());
                velocitySkinnedMeshPSO->SetPS(gShaders[L"TaaVelocityPS"].get());
                velocitySkinnedMeshPSO->Finalize(device);

            
                gPSOs[L"VelocitySkinned"] = std::move(velocitySkinnedMeshPSO);
            }

            if (gPSOs[L"TaaOutput"] == 0)
            {
                auto outputMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
                outputMeshPSO->SetRootSignature(sRootSignature.get());
                outputMeshPSO->SetDepthStencilState(gDepthDisabledState);
                outputMeshPSO->SetRenderTargetFormat(mFrameFormat);
                outputMeshPSO->SetMS(gShaders[L"ScreenMS"].get());
                outputMeshPSO->SetPS(gShaders[L"TaaOutputPS"].get());
                outputMeshPSO->Finalize(device);
            
                gPSOs[L"TaaOutput"] = std::move(outputMeshPSO);
            }
        }

        virtual void InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)override
        {
            mHistFrameMap = make_unique<ColorBuffer>(
                mWidth,
                mHeight,
                1,
                COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
                mFrameFormat,
                device,
                heap,
                descriptorManager,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            
            D3D12_CLEAR_VALUE optClearValue = CD3DX12_CLEAR_VALUE(mVelocityMapFormat, DirectX::Colors::Black);
            mVelocityMap = make_unique<ColorBuffer>(
                mWidth,
                mHeight,
                1,
                COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
                mVelocityMapFormat,
                device,
                heap,
                descriptorManager,
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

        void DrawVelocityMap(ID3D12GraphicsCommandList* cmdList)
        {
            cmdList->RSSetViewports(1, &mViewport);
            cmdList->RSSetScissorRects(1, &mScissorRect);

            mVelocityMap->Transition(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
            cmdList->ClearRenderTargetView(mVelocityMap->GetRtv(), DirectX::Colors::Black, 0, nullptr);
            cmdList->ClearDepthStencilView(mFrameDsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);
            cmdList->OMSetRenderTargets(1, GetRvaluePtr(mVelocityMap->GetRtv()), true, &mFrameDsv);

            cmdList->SetPipelineState(gPSOs[L"VelocityStatic"]->Get());
            ExecuteIndirect(cmdList, mIndirectCommandBuffer[OPAQUE_STATIC]);
            ExecuteIndirect(cmdList, mIndirectCommandBuffer[TRANSPARENT_STATIC]);

            cmdList->SetPipelineState(gPSOs[L"VelocitySkinned"]->Get());
            ExecuteIndirect(cmdList, mIndirectCommandBuffer[OPAQUE_SKINNED]);
            ExecuteIndirect(cmdList, mIndirectCommandBuffer[TRANSPARENT_SKINNED]);

            mVelocityMap->Transition(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        void DrawOutput(ID3D12GraphicsCommandList* cmdList)
        {
            mCurrBackBuffer->Transition(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

            cmdList->ClearRenderTargetView(mCurrBackBufferRtv, DirectX::Colors::Gray, 0, nullptr);
            cmdList->OMSetRenderTargets(1, &mCurrBackBufferRtv, true, nullptr);
            cmdList->SetPipelineState(gPSOs[L"TaaOutput"]->Get());
            static_cast<ID3D12GraphicsCommandList6*>(cmdList)->DispatchMesh(1, 1, 1);

            mHistFrameMap->Transition(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);
            mCurrBackBuffer->Transition(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE);
            cmdList->CopyResource(mHistFrameMap->Get(), mCurrBackBuffer->Get());
            mHistFrameMap->Transition(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            mCurrBackBuffer->Transition(cmdList, D3D12_RESOURCE_STATE_PRESENT);
        }

		unique_ptr<ColorBuffer> mHistFrameMap;
		unique_ptr<ColorBuffer> mVelocityMap;

		DXGI_FORMAT mVelocityMapFormat;
		DXGI_FORMAT mFrameFormat;
		DXGI_FORMAT mFrameDsvFormat;

		D3D12_CPU_DESCRIPTOR_HANDLE mCurrBackBufferRtv;
		D3D12_CPU_DESCRIPTOR_HANDLE mFrameDsv;

		XMFLOAT2 mHalton[8];
		XMMATRIX mHistViewProj;
		
		vector<const StructuredBuffer*> mIndirectCommandBuffer;
		Resource* mCurrBackBuffer;
	};
}