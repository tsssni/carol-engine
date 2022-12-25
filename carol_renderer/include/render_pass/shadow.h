#pragma once
#include <render_pass/render_pass.h>
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

    class ShadowPass : public RenderPass
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

        CD3DX12_GPU_DESCRIPTOR_HANDLE GetShadowSrv();
        const Light& GetLight();
    protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
        virtual void InitResources()override;
        virtual void InitDescriptors()override;

        void InitLightView();
        void InitCamera();

        std::unique_ptr<Light> mLight;
        std::unique_ptr<DefaultResource> mShadowMap;
        
        enum
        {
            SHADOW_SRV, SHADOW_SRV_COUNT
        };

        enum
        {
            SHADOW_DSV, SHADOW_DSV_COUNT
        };

        D3D12_VIEWPORT mViewport;
        D3D12_RECT mScissorRect;
        std::unique_ptr<Camera> mCamera;
        uint32_t mWidth;
        uint32_t mHeight;

        DXGI_FORMAT mShadowFormat;
        DXGI_FORMAT mShadowDsvFormat;
        DXGI_FORMAT mShadowSrvFormat;
    };
}
