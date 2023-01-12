#pragma once
#include <scene/light.h>
#include <base_renderer.h>
#include <memory>
#include <unordered_map>
#include <string>

#define MAX_LIGHTS 16

namespace Carol
{
	class FastConstantBufferAllocator;
	class Scene;
    class FramePass;
    class SsaoPass;
    class NormalPass;
    class TaaPass;
    class ShadowPass;
    class OitppllPass;
    class MeshesPass;
    class TextureManager;

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

		uint32_t MeshCBIdx;
		uint32_t CommandBufferIdx;
		uint32_t InstanceFrustumCulledMarkIdx;
		uint32_t InstanceOcclusionPassedMarkIdx;

		uint32_t FrameMapIdx;
		uint32_t DepthStencilMapIdx;
		uint32_t NormalMapIdx;
		uint32_t MainLightShadowMapIdx;
		uint32_t OitBufferWIdx;
		uint32_t OitOffsetBufferWIdx;
		uint32_t OitCounterIdx;
		uint32_t OitBufferRIdx;
		uint32_t OitOffsetBufferRIdx;
		uint32_t RandVecMapIdx;
		uint32_t AmbientMapIdx;
		uint32_t VelocityMapIdx;
		uint32_t HistFrameMapIdx;
		DirectX::XMFLOAT3 FramePad2;
	};
 
    class Renderer :public BaseRenderer
    {
    public:
		Renderer(HWND hWnd, uint32_t width, uint32_t height);
        
        virtual void Draw()override;
        virtual void Update()override;

        virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
		virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
		virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
		virtual void OnKeyboardInput()override;
        virtual void OnResize(uint32_t width, uint32_t height, bool init = false)override;

        void LoadModel(std::wstring path, std::wstring textureDir, std::wstring modelName, DirectX::XMMATRIX world, bool isSkinned);
        void UnloadModel(std::wstring modelName);
        std::vector<std::wstring> GetAnimationNames(std::wstring modelName);
        void SetAnimation(std::wstring modelName, std::wstring animationName);
        std::vector<std::wstring> GetModelNames();
    protected:
		void InitConstants();
        void InitPSOs();
        void InitFrame();
        void InitSsao();
        void InitNormal();
        void InitTaa();
        void InitMainLight();
        void InitOitppll();
        void InitMeshes();
		
		void UpdateFrameCB();
		void DelayedDelete();
        void ReleaseIntermediateBuffers();

    protected:

		std::unique_ptr<Scene> mScene;
        std::unique_ptr<FramePass> mFrame;
        std::unique_ptr<SsaoPass> mSsao;
        std::unique_ptr<NormalPass> mNormal;
        std::unique_ptr<TaaPass> mTaa;
        std::unique_ptr<ShadowPass> mMainLight;
        std::unique_ptr<OitppllPass> mOitppll;
        std::unique_ptr<MeshesPass> mMeshes;

		std::unique_ptr<FrameConstants> mFrameConstants;
        std::unique_ptr<FastConstantBufferAllocator> mFrameCBAllocator;
		D3D12_GPU_VIRTUAL_ADDRESS mFrameCBAddr;
    };

}

