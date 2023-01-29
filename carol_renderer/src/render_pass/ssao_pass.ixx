export module carol.renderer.render_pass.ssao_pass;
import carol.renderer.render_pass.render_pass;
import carol.renderer.render_pass.frame_pass;
import carol.renderer.dx12;
import carol.renderer.scene;
import carol.renderer.utils;
import <DirectXMath.h>;
import <DirectXColors.h>;
import <DirectXPackedVector.h>;
import <memory>;
import <vector>;
import <string_view>;

#define BLUR_RADIUS 5

namespace Carol
{
    using Microsoft::WRL::ComPtr;
    using std::make_unique;
    using std::unique_ptr;
    using std::vector;
    using std::wstring;
    using std::wstring_view;
    using namespace DirectX;
    using namespace DirectX::PackedVector;

    export class SsaoPass : public RenderPass
    {
    public:
        SsaoPass(
            uint32_t blurCount = 3,
            DXGI_FORMAT ambientMapFormat = DXGI_FORMAT_R16_UNORM)
             :mBlurCount(blurCount), 
            mAmbientMapFormat(ambientMapFormat)
        {
            InitShaders();
            InitPSOs();
            InitRandomVectors();
            InitRandomVectorMap();
        }

        SsaoPass(const SsaoPass &) = delete;
        SsaoPass(SsaoPass &&) = delete;
        SsaoPass &operator=(const SsaoPass &) = delete;

        virtual void Draw() override
        {
            uint32_t groupWidth = ceilf(mWidth * 1.f / (32 - 2 * BLUR_RADIUS)); 
            uint32_t groupHeight = ceilf(mHeight * 1.f / (32 - 2 * BLUR_RADIUS));

            gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mAmbientMap->Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)));

            gCommandList->SetPipelineState(gPSOs[L"Ssao"]->Get());
            gCommandList->Dispatch(groupWidth, groupHeight , 1);

            gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mAmbientMap->Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));
        }

        virtual void Update() override
        {

        }

        virtual void ReleaseIntermediateBuffers() override
        {
            mRandomVecMap->ReleaseIntermediateBuffer();
        }

        virtual void OnResize(uint32_t width, uint32_t height) override
        {
            if (mWidth != width >> 1 || mHeight != height >> 1)
            {
                mWidth = width >> 1;
                mHeight = height >> 1;
                mViewport = { 0.f,0.f,mWidth * 1.f,mHeight * 1.f,0.f,1.f };
                mScissorRect = { 0,0,(long)mWidth,(long)mHeight };

                InitBuffers();
            }
        }

        std::vector<float> CalcGaussWeights(float sigma)
        {
            static int maxGaussRadius = 5;

            int blurRadius = (int)ceil(2.0f * sigma);
            assert(blurRadius <= maxGaussRadius);

            vector<float> weights;
            float weightsSum = 0.0f;

            weights.resize(2 * blurRadius + 1);

            for (int i = -blurRadius; i <= blurRadius; ++i)
            {
                weights[blurRadius + i] = expf(-i * i / (2.0f * sigma * sigma));
                weightsSum += weights[blurRadius + i];
            }

            for (int i = 0; i < blurRadius * 2 + 1; ++i)
            {
                weights[i] /= weightsSum;
            }
            
            return weights;
        }

        void GetOffsetVectors(XMFLOAT4 offsets[14])
        {
            std::copy(&mOffsets[0], &mOffsets[14], offsets);
        }

        uint32_t GetRandVecSrvIdx()
        {
            return mRandomVecMap->GetGpuSrvIdx();
        }

        uint32_t GetSsaoSrvIdx()
        {
            return mAmbientMap->GetGpuSrvIdx();
        }

        uint32_t GetSsaoUavIdx()
        {
            return mAmbientMap->GetGpuUavIdx();
        }

    protected:
        virtual void InitShaders() override
        {
            vector<wstring_view> nullDefines{};

            gShaders[L"SsaoCS"] = make_unique<Shader>(L"shader\\ssao_cs.hlsl", nullDefines, L"main", L"cs_6_6");
        }

        virtual void InitPSOs() override
        {
            auto ssaoComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
            ssaoComputePSO->SetCS(gShaders[L"SsaoCS"].get());
            ssaoComputePSO->Finalize();
            gPSOs[L"Ssao"] = std::move(ssaoComputePSO);
        }
        
        virtual void InitBuffers() override
        {
            mAmbientMap = make_unique<ColorBuffer>(
                mWidth,
                mHeight,
                1,
                COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
                mAmbientMapFormat,
                gHeapManager->GetDefaultBuffersHeap(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        }

        void InitRandomVectors()
        {
            mOffsets[0] = XMFLOAT4(+1.0f, +1.0f, +1.0f, 0.0f);
            mOffsets[1] = XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);

            mOffsets[2] = XMFLOAT4(-1.0f, +1.0f, +1.0f, 0.0f);
            mOffsets[3] = XMFLOAT4(+1.0f, -1.0f, -1.0f, 0.0f);

            mOffsets[4] = XMFLOAT4(+1.0f, +1.0f, -1.0f, 0.0f);
            mOffsets[5] = XMFLOAT4(-1.0f, -1.0f, +1.0f, 0.0f);

            mOffsets[6] = XMFLOAT4(-1.0f, +1.0f, -1.0f, 0.0f);
            mOffsets[7] = XMFLOAT4(+1.0f, -1.0f, +1.0f, 0.0f);

            // 6 centers of cube faces
            mOffsets[8] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
            mOffsets[9] = XMFLOAT4(+1.0f, 0.0f, 0.0f, 0.0f);

            mOffsets[10] = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
            mOffsets[11] = XMFLOAT4(0.0f, +1.0f, 0.0f, 0.0f);

            mOffsets[12] = XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
            mOffsets[13] = XMFLOAT4(0.0f, 0.0f, +1.0f, 0.0f);

            for (int i = 0; i < 14; ++i)
            {
                float s = 0.25f + rand() * 1.0f / RAND_MAX * (1.0f - 0.25f);
                XMVECTOR v = s * XMVector4Normalize(XMLoadFloat4(&mOffsets[i]));

                XMStoreFloat4(&mOffsets[i], v);
            }
        }

        void InitRandomVectorMap()
        { 
            mRandomVecMap = make_unique<ColorBuffer>(
                256,
                256,
                1,
                COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                gHeapManager->GetDefaultBuffersHeap(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            XMCOLOR initData[256 * 256];
            for (int i = 0; i < 256; ++i)
            {
                for (int j = 0; j < 256; ++j)
                {
                    XMCOLOR randVec = { rand() * 1.0f / RAND_MAX,rand() * 1.0f / RAND_MAX,rand() * 1.0f / RAND_MAX ,0.0f };
                    initData[i * 256 + j] = randVec;
                }
            }

            D3D12_SUBRESOURCE_DATA subresource;
            subresource.pData = initData;
            subresource.RowPitch = 256 * sizeof(XMCOLOR);
            subresource.SlicePitch = subresource.RowPitch * 256;
            mRandomVecMap->CopySubresources(gHeapManager->GetUploadBuffersHeap(), &subresource, 0, 1, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        unique_ptr<ColorBuffer> mRandomVecMap;
        unique_ptr<ColorBuffer> mAmbientMap;

        DXGI_FORMAT mAmbientMapFormat = DXGI_FORMAT_R16_UNORM;
        uint32_t mBlurCount;
        XMFLOAT4 mOffsets[14];
    };
}