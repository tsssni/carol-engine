#pragma once
#include <render_pass/render_pass.h>
#include <scene/light.h>
#include <scene/mesh.h>
#include <DirectXMath.h>
#include <vector>
#include <memory>
#include <span>

#define MAX_LIGHTS 16

namespace Carol
{
    class ColorBuffer;
    class StructuredBuffer;
    class StructuredBufferPool;
    class Scene;
    class Camera; 
    class PerspectiveCamera;
    class Heap;
    class DescriptorManager;
    class CullPass;

    class ShadowPass : public RenderPass
    {
    public:
        ShadowPass(
            Light light,
            uint32_t width = 1024,
            uint32_t height = 1024,
            uint32_t depthBias = 60000,
            float depthBiasClamp = 0.01f,
            float slopeScaledDepthBias = 4.f,
            DXGI_FORMAT shadowFormat = DXGI_FORMAT_R32_FLOAT,
            DXGI_FORMAT hiZFormat = DXGI_FORMAT_R32_FLOAT);

        virtual void Draw()override;
        void Update(uint32_t lightIdx);

        uint32_t GetShadowSrvIdx()const;
        const Light& GetLight()const;

    protected:
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;
        
        void InitLight();
        virtual void InitCamera() = 0;
                
        std::unique_ptr<Light> mLight;
        std::unique_ptr<ColorBuffer> mShadowMap;

        uint32_t mDepthBias;
        float mDepthBiasClamp;
        float mSlopeScaledDepthBias;

        std::unique_ptr<Camera> mCamera;

        DXGI_FORMAT mShadowFormat;

        std::unique_ptr<CullPass> mCullPass;
    };

    class DirectLightShadowPass : public ShadowPass
    {
    public:
        DirectLightShadowPass(
            Light light,
            uint32_t width = 1024,
            uint32_t height = 1024,
            uint32_t depthBias = 60000,
            float depthBiasClamp = 0.01f,
            float slopeScaledDepthBias = 4.f,
            DXGI_FORMAT shadowFormat = DXGI_FORMAT_R32_FLOAT,
            DXGI_FORMAT hiZFormat = DXGI_FORMAT_R32_FLOAT);

        void Update(uint32_t lightIdx, const PerspectiveCamera* camera, float zn, float zf);
    protected:
        virtual void InitCamera()override;
    };

    class CascadedShadowPass : public RenderPass
    {
	public:
        CascadedShadowPass(
			Light light,
            uint32_t splitLevel = 5,
            uint32_t width = 1024,
            uint32_t height = 1024,
            uint32_t depthBias = 60000,
            float depthBiasClamp = 0.01f,
            float slopeScaledDepthBias = 4.f,
            DXGI_FORMAT shadowFormat = DXGI_FORMAT_R32_FLOAT,
            DXGI_FORMAT hiZFormat = DXGI_FORMAT_R32_FLOAT);

		virtual void Draw()override;
        void Update(const PerspectiveCamera* camera, float logWeight = 0.5f, float bias = 0.f);
        
        uint32_t GetSplitLevel()const;
        float GetSplitZ(uint32_t idx)const;
        
        uint32_t GetShadowSrvIdx(uint32_t idx)const;
        const Light& GetLight(uint32_t idx)const;
	protected:
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;

        uint32_t mSplitLevel;
        std::vector<float> mSplitZ;
        std::vector<std::unique_ptr<DirectLightShadowPass>> mShadow;
    };
}
