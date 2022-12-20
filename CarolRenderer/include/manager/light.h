#pragma once
#include <manager/manager.h>
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

    enum
    {
        DIR_LIGHT,POINT_LIGHT,SPOT_LIGHT
    };

	class LightData
	{
    public:
        DirectX::XMFLOAT3 Strength = { 0.3f, 0.3f, 0.3f };
        float FalloffStart = 1.0f;                          
        DirectX::XMFLOAT3 Direction = { 1.0f, -1.0f, 1.0f };
        float FalloffEnd = 10.0f;                  
        DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f }; 
        float SpotPower = 64.0f;
        DirectX::XMFLOAT3 Ambient = { 0.4f,0.4f,0.4f };
        float LightPad0;

        DirectX::XMFLOAT4X4 ViewProjTex;
	};

    class ShadowConstants
    {
    public:
        DirectX::XMFLOAT4X4 LightViewProj;
    };

    class Light
    {
    public:
        LightData LightData;
        uint32_t Width;
        uint32_t Height;
    };
    

    class LightManager : public Manager
    {
    public:
        LightManager(
            GlobalResources* globalResources,
            LightData lightData,
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

        CD3DX12_GPU_DESCRIPTOR_HANDLE GetShadowSrv();
        const LightData& GetLightData();
    protected:
        virtual void InitRootSignature()override;
		virtual void InitShaders()override;
		virtual void InitPSOs()override;
        virtual void InitResources()override;
        virtual void InitDescriptors()override;

        void InitConstants();
        void InitLightView();
        void InitCamera();

        void DrawAllMeshes();

        std::unique_ptr<LightData> mLightData;
        std::unique_ptr<Light> mMainLight;
        std::unique_ptr<DefaultResource> mShadowMap;

        enum
        {
            SHADOW_SRV, LIGHT_SRV_COUNT
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
