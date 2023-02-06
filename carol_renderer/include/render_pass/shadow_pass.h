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
    class Camera; 
    class PerspectiveCamera;

    class ShadowPass : public RenderPass
    {
    public:
        ShadowPass(
            Light light,
            uint32_t width = 512,
            uint32_t height = 512,
            uint32_t depthBias = 60000,
            float depthBiasClamp = 0.01f,
            float slopeScaledDepthBias = 4.f,
            DXGI_FORMAT shadowFormat = DXGI_FORMAT_R32_FLOAT,
            DXGI_FORMAT hiZFormat = DXGI_FORMAT_R32_FLOAT);

        virtual void Draw()override;
        virtual void Update(uint32_t lightIdx);

        uint32_t GetShadowSrvIdx();
        const Light& GetLight();

    protected:
        virtual void Update()override;
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
        virtual void InitBuffers()override;
        
        void InitLightViewport();
        virtual void InitCamera() = 0;
        
        void Clear();
        void CullInstances(bool hist);
        void DrawShadow(bool hist);
        void GenerateHiZ();

		void TestCommandBufferSize(std::unique_ptr<StructuredBuffer>& buffer, uint32_t numElements);
		void ResizeCommandBuffer(std::unique_ptr<StructuredBuffer>& buffer, uint32_t numElements, uint32_t elementSize);
        
        enum
        {
            HIZ_DEPTH_IDX,
            HIZ_R_IDX,
            HIZ_W_IDX,
            HIZ_SRC_MIP,
            HIZ_NUM_MIP_LEVEL,
            HIZ_IDX_COUNT
        };

        enum
        {
            CULL_CULLED_COMMAND_BUFFER_IDX,
            CULL_MESH_COUNT,
            CULL_MESH_OFFSET,
            CULL_HIZ_IDX,
            CULL_HIST,
            CULL_LIGHT_IDX,
            CULL_IDX_COUNT
        };
        
        std::unique_ptr<Light> mLight;
        std::unique_ptr<ColorBuffer> mShadowMap;
        std::unique_ptr<ColorBuffer> mHiZMap;
        std::vector<std::vector<std::unique_ptr<StructuredBuffer>>> mCulledCommandBuffer;

        uint32_t mDepthBias;
        float mDepthBiasClamp;
        float mSlopeScaledDepthBias;

        std::vector<std::vector<uint32_t>> mCullIdx;
        std::vector<uint32_t> mHiZIdx;
		uint32_t mHiZMipLevels;

        std::unique_ptr<Camera> mCamera;

        DXGI_FORMAT mShadowFormat;
        DXGI_FORMAT mHiZFormat;
    };

    class DirectLightShadowPass : public ShadowPass
    {
    public:
		DirectLightShadowPass(
		Light light,
		uint32_t width = 512,
		uint32_t height = 512,
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
            uint32_t width = 512,
            uint32_t height = 512,
            uint32_t depthBias = 60000,
            float depthBiasClamp = 0.01f,
            float slopeScaledDepthBias = 4.f,
            DXGI_FORMAT shadowFormat = DXGI_FORMAT_R32_FLOAT,
            DXGI_FORMAT hiZFormat = DXGI_FORMAT_R32_FLOAT);

		virtual void Draw();
        void Update(const PerspectiveCamera* camera, float logWeight = 0.5f, float bias = 0.f);
        
        uint32_t GetSplitLevel();
        float GetSplitZ(uint32_t idx);
        
        uint32_t GetShadowSrvIdx(uint32_t idx);
        const Light& GetLight(uint32_t idx);
	protected:
        virtual void Update();
		virtual void InitShaders();
		virtual void InitPSOs();
		virtual void InitBuffers();

        uint32_t mSplitLevel;
        std::vector<float> mSplitZ;
        std::vector<std::unique_ptr<DirectLightShadowPass>> mShadow;
    };
}
