#pragma once
#include <render/pass.h>
#include <scene/light.h>
#include <DirectXMath.h>
#include <vector>
#include <memory>

#define MAX_LIGHTS 16

namespace Carol
{
    class GlobalResource;
    class DefaultResource;
    class HeapAllocInfo;
    class CircularHeap;
    class Shader;
    class Camera;

	class ShadowConstants
    {
    public:
        uint32_t LightIdx;
        DirectX::XMUINT3 ShadowPad0;
    };    

    class ShadowPass : public Pass
    {
    public:
        ShadowPass(
            GlobalResources* globalResources,
            Light light,
            uint32_t width = 1024,
            uint32_t height = 1024,
            DXGI_FORMAT shadowFormat = DXGI_FORMAT_R32_TYPELESS,
            DXGI_FORMAT shadowDsvFormat = DXGI_FORMAT_D32_FLOAT,
            DXGI_FORMAT shadowSrvFormat = DXGI_FORMAT_R32_FLOAT);

        virtual void Draw()override;
        virtual void Update()override;
        virtual void OnResize()override;
        virtual void ReleaseIntermediateBuffers()override;

        static void InitShadowCBHeap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);

        uint32_t GetShadowSrvIdx();
        const Light& GetLight();
    protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
        virtual void InitResources()override;
        virtual void InitDescriptors()override;

        void InitConstants();
        void InitLightView();
        void InitCamera();

        void DrawAllMeshes();

        std::unique_ptr<Light> mLight;
        std::unique_ptr<DefaultResource> mShadowMap;

        enum
        {
            SHADOW_TEX2D_SRV, LIGHT_TEX2D_SRV_COUNT
        };

        enum
        {
            SHADOW_DSV, LIGHT_DSV_COUNT
        };

        D3D12_VIEWPORT mViewport;
        D3D12_RECT mScissorRect;
        std::unique_ptr<Camera> mCamera;
        uint32_t mWidth;
        uint32_t mHeight;

        DXGI_FORMAT mShadowFormat;
        DXGI_FORMAT mShadowDsvFormat;
        DXGI_FORMAT mShadowSrvFormat;

        std::unique_ptr<ShadowConstants> mShadowConstants;
        std::unique_ptr<HeapAllocInfo> mShadowCBAllocInfo;
        static std::unique_ptr<CircularHeap> ShadowCBHeap;
    };
}
