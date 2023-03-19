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

    class ShadowPass : public RenderPass
    {
    public:
        ShadowPass(
            ID3D12Device* device,
            Heap* heap,
            DescriptorManager* descriptorManager,
            Scene* scene,
            Light light,
            uint32_t width = 1024,
            uint32_t height = 1024,
            uint32_t depthBias = 60000,
            float depthBiasClamp = 0.01f,
            float slopeScaledDepthBias = 4.f,
            DXGI_FORMAT shadowFormat = DXGI_FORMAT_R32_FLOAT,
            DXGI_FORMAT hiZFormat = DXGI_FORMAT_R32_FLOAT);

        virtual void Draw(ID3D12GraphicsCommandList* cmdList)override;
        void Update(uint32_t lightIdx, uint64_t cpuFenceValue, uint64_t completedFenceValue);

        uint32_t GetShadowSrvIdx()const;
        const Light& GetLight()const;

    protected:
		virtual void InitShaders()override;
		virtual void InitPSOs(ID3D12Device* device)override;
		virtual void InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager);
        
        void InitLight();
        virtual void InitCamera() = 0;
        
        void Clear(ID3D12GraphicsCommandList* cmdList);
        void CullInstances(uint32_t iteration, ID3D12GraphicsCommandList* cmdList);
        void DrawShadow(uint32_t iteration, ID3D12GraphicsCommandList* cmdList);
        void GenerateHiZ(ID3D12GraphicsCommandList* cmdList);
        
        enum
        {
            HIZ_DEPTH_IDX,
            HIZ_IDX,
            RW_HIZ_IDX,
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
            CULL_ITERATION,
            CULL_LIGHT_IDX,
            CULL_IDX_COUNT
        };
        
        std::unique_ptr<Light> mLight;
        std::unique_ptr<ColorBuffer> mShadowMap;
        std::unique_ptr<ColorBuffer> mHiZMap;

        std::vector<std::unique_ptr<StructuredBuffer>> mCulledCommandBuffer;
        std::unique_ptr<StructuredBufferPool> mCulledCommandBufferPool;

        Scene* mScene;

        uint32_t mDepthBias;
        float mDepthBiasClamp;
        float mSlopeScaledDepthBias;

        std::vector<std::vector<uint32_t>> mCullIdx;
        std::vector<uint32_t> mHiZIdx;

        std::unique_ptr<Camera> mCamera;

        DXGI_FORMAT mShadowFormat;
        DXGI_FORMAT mHiZFormat;
    };

    class DirectLightShadowPass : public ShadowPass
    {
    public:
        DirectLightShadowPass(
            ID3D12Device* device,
            Heap* heap,
            DescriptorManager* descriptorManager,
            Scene* scene,
            Light light,
            uint32_t width = 1024,
            uint32_t height = 1024,
            uint32_t depthBias = 60000,
            float depthBiasClamp = 0.01f,
            float slopeScaledDepthBias = 4.f,
            DXGI_FORMAT shadowFormat = DXGI_FORMAT_R32_FLOAT,
            DXGI_FORMAT hiZFormat = DXGI_FORMAT_R32_FLOAT);

        void Update(uint32_t lightIdx, const PerspectiveCamera* camera, float zn, float zf, uint64_t cpuFenceValue, uint64_t completedFenceValue);
    protected:
        virtual void InitCamera()override;
    };

    class CascadedShadowPass : public RenderPass
    {
	public:
        CascadedShadowPass(
            ID3D12Device* device,
            Heap* heap,
            DescriptorManager* descriptorManager,
            Scene* scene,
            Light light,
            uint32_t splitLevel = 5,
            uint32_t width = 1024,
            uint32_t height = 1024,
            uint32_t depthBias = 60000,
            float depthBiasClamp = 0.01f,
            float slopeScaledDepthBias = 4.f,
            DXGI_FORMAT shadowFormat = DXGI_FORMAT_R32_FLOAT,
            DXGI_FORMAT hiZFormat = DXGI_FORMAT_R32_FLOAT);

		virtual void Draw(ID3D12GraphicsCommandList* cmdList)override;
        void Update(const PerspectiveCamera* camera, uint64_t cpuFenceValue, uint64_t completedFenceValue, float logWeight = 0.5f, float bias = 0.f);
        
        uint32_t GetSplitLevel()const;
        float GetSplitZ(uint32_t idx)const;
        
        uint32_t GetShadowSrvIdx(uint32_t idx)const;
        const Light& GetLight(uint32_t idx)const;
	protected:
		virtual void InitShaders()override;
		virtual void InitPSOs(ID3D12Device* device)override;
		virtual void InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)override;

        uint32_t mSplitLevel;
        std::vector<float> mSplitZ;
        std::vector<std::unique_ptr<DirectLightShadowPass>> mShadow;
    };
}
