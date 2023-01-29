#pragma once
#include <scene/light.h>
#include <base_renderer.h>
#include <memory>
#include <unordered_map>
#include <string>
#include <string_view>

#define MAX_LIGHTS 16

namespace Carol
{
	class FastConstantBufferAllocator;
    class FramePass;
    class SsaoPass;
    class NormalPass;
    class TaaPass;
    class ShadowPass;
    class MeshesPass;

	class FrameConstants
	{
	public:
		// Transformation
		DirectX::XMFLOAT4X4 View;
		DirectX::XMFLOAT4X4 InvView;
		DirectX::XMFLOAT4X4 Proj;
		DirectX::XMFLOAT4X4 InvProj;
		DirectX::XMFLOAT4X4 ViewProj;
		DirectX::XMFLOAT4X4 InvViewProj;
		DirectX::XMFLOAT4X4 ProjTex;
		DirectX::XMFLOAT4X4 ViewProjTex;
		DirectX::XMFLOAT4X4 HistViewProj;
		DirectX::XMFLOAT4X4 JitteredViewProj;

		// View
		DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
		float FramePad0 = 0.0f;
		DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
		DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
		float NearZ = 0.0f;
		float FarZ = 0.0f;
		DirectX::XMFLOAT2 FramePad1;

		// Ssao
		DirectX::XMFLOAT4 OffsetVectors[14];
		DirectX::XMFLOAT4 BlurWeights[3];
		float OcclusionRadius = 0.5f;
		float OcclusionFadeStart = 0.2f;
		float OcclusionFadeEnd = 1.0f;
		float SurfaceEplison = 0.05f;

		Light Lights[MAX_LIGHTS];

		uint32_t MeshCBIdx = 0;
		uint32_t CommandBufferIdx = 0;
		uint32_t InstanceFrustumCulledMarkIdx = 0;
		uint32_t InstanceOcclusionPassedMarkIdx = 0;

		uint32_t FrameMapIdx = 0;
		uint32_t DepthStencilMapIdx = 0;
		uint32_t NormalMapIdx = 0;
		uint32_t MainLightShadowMapIdx = 0;
		// OITPPLL
		uint32_t OitBufferWIdx = 0;
		uint32_t OitOffsetBufferWIdx = 0;
		uint32_t OitCounterIdx = 0;
		uint32_t OitBufferRIdx = 0;
		uint32_t OitOffsetBufferRIdx = 0;
		// SSAO
		uint32_t RandVecMapIdx = 0;
		uint32_t AmbientMapWIdx = 0;
		uint32_t AmbientMapRIdx = 0;
		// TAA
		uint32_t VelocityMapIdx = 0;
		uint32_t HistFrameMapIdx = 0;

		DirectX::XMFLOAT2 FramePad2;
	};
 
    class Renderer :public BaseRenderer
    {
    public:
		Renderer(HWND hWnd, uint32_t width, uint32_t height);
		~Renderer();
        
        virtual void Draw()override;
        virtual void Update()override;

        virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
		virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
		virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
		virtual void OnKeyboardInput()override;
        virtual void OnResize(uint32_t width, uint32_t height, bool init = false)override;

        void LoadModel(std::wstring_view path, std::wstring_view textureDir, std::wstring_view modelName, DirectX::XMMATRIX world, bool isSkinned);
        void UnloadModel(std::wstring_view modelName);
        std::vector<std::wstring_view> GetAnimationNames(std::wstring_view modelName);
        void SetAnimation(std::wstring_view modelName, std::wstring_view animationName);
        std::vector<std::wstring_view> GetModelNames();
    protected:
		void InitConstants();
        void InitPipelineStates();

        void InitFrame();
        void InitSsao();
        void InitNormal();
        void InitTaa();
        void InitMainLight();
        void InitScene();
		
		void UpdateFrameCB();
        void ReleaseIntermediateBuffers();

    protected:
        std::unique_ptr<SsaoPass> mSsaoPass;
        std::unique_ptr<NormalPass> mNormalPass;
        std::unique_ptr<TaaPass> mTaaPass;
        std::unique_ptr<ShadowPass> mMainLightShadowPass;

		std::unique_ptr<FrameConstants> mFrameConstants;
        std::unique_ptr<FastConstantBufferAllocator> mFrameCBAllocator;
		D3D12_GPU_VIRTUAL_ADDRESS mFrameCBAddr;
    };

}

