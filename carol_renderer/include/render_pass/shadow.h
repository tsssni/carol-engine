#pragma once
#include <render_pass/render_pass.h>
#include <scene/light.h>
#include <scene/model.h>
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
        
        void Clear();
        void CullMeshes(bool hist);
        void DrawShadow(bool hist);
        void DrawHiZ();

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

        std::vector<std::vector<uint32_t>> mCullIdx;
        std::vector<uint32_t> mHiZIdx;
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
