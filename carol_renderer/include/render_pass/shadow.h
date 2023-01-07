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
    class ColorBuffer;
    class StructuredBuffer;
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
            DXGI_FORMAT shadowFormat = DXGI_FORMAT_R32_FLOAT,
            DXGI_FORMAT hiZFormat = DXGI_FORMAT_R32_FLOAT);

        virtual void Draw()override;
        virtual void Update()override;
        virtual void OnResize()override;
        virtual void ReleaseIntermediateBuffers()override;

        uint32_t GetShadowSrvIdx();
        const Light& GetLight();
    protected:
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
        virtual void InitBuffers()override;
        
        void InitLightView();
        void InitCamera();
        
        void CullMeshes();
        void DrawShadow();
        void DrawHiZ();

        void UpdateLight();
        void UpdateCommandBuffer();

        std::unique_ptr<Light> mLight;
        std::unique_ptr<ColorBuffer> mShadowMap;
        std::unique_ptr<ColorBuffer> mHiZMap;
        std::vector<std::unique_ptr<StructuredBuffer>> mOcclusionCommandBuffer;
		uint32_t mHiZMipLevels;

        D3D12_VIEWPORT mViewport;
        D3D12_RECT mScissorRect;
        std::unique_ptr<Camera> mCamera;
        uint32_t mWidth;
        uint32_t mHeight;

        DXGI_FORMAT mShadowFormat;
        DXGI_FORMAT mHiZFormat;
    };
}
