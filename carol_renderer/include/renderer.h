#pragma once
#include <scene/light.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>
#include <unordered_map>
#include <string>
#include <string_view>

#define MAIN_LIGHT_SPLIT_LEVEL 5
#define MAX_POINT_LIGHTS 64
#define MAX_SPOT_LIGHTS 64

namespace Carol
{
	class FastConstantBufferAllocator;
	class Camera;
	class Timer;
	class Scene;
	class DisplayPass;
    class FramePass;
    class NormalPass;
    class CascadedShadowPass;
    class SsaoPass;
    class TaaPass;
	class ToneMappingPass;

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
		DirectX::XMFLOAT4 GaussBlurWeights[3];
		float OcclusionRadius = 0.5f;
		float OcclusionFadeStart = 0.2f;
		float OcclusionFadeEnd = 1.0f;
		float SurfaceEplison = 0.05f;

		Light MainLights[MAIN_LIGHT_SPLIT_LEVEL];
		float MainLightSplitZ[MAIN_LIGHT_SPLIT_LEVEL + 1];
		DirectX::XMFLOAT2 FramePad2;
		DirectX::XMFLOAT3 AmbientColor;
		float FramePad3;

		uint32_t NumPointLights = 0;
		DirectX::XMFLOAT3 FramePad4;
		Light PointLights[MAX_POINT_LIGHTS];

		uint32_t NumSpotLights = 0;
		DirectX::XMFLOAT3 FramePad5;
		Light SpotLights[MAX_POINT_LIGHTS];

		// Main light
		uint32_t MainLightShadowMapIdx[MAIN_LIGHT_SPLIT_LEVEL] = { 0 };
		DirectX::XMFLOAT3 FramePad6;

		uint32_t MeshBufferIdx = 0;
		uint32_t CommandBufferIdx = 0;

		uint32_t InstanceFrustumCulledMarkBufferIdx = 0;
		uint32_t InstanceOcclusionCulledMarkBufferIdx = 0;
		uint32_t InstanceCulledMarkBufferIdx = 0;

		uint32_t DepthStencilMapIdx = 0;
		uint32_t NormalMapIdx = 0;

		// OITPPLL
		uint32_t OitCounterIdx = 0;
		uint32_t RWOitBufferIdx = 0;
		uint32_t RWOitOffsetBufferIdx = 0;
		uint32_t OitBufferIdx = 0;
		uint32_t OitOffsetBufferIdx = 0;

		// SSAO
		uint32_t RandVecMapIdx = 0;
		uint32_t RWAmbientMapIdx = 0;
		uint32_t AmbientMapIdx = 0;
		
		// TAA
		uint32_t VelocityMapIdx = 0;
		uint32_t RWHistMapIdx = 0;
		uint32_t RWFrameMapIdx = 0;

		DirectX::XMFLOAT2 FramePad7;
	};
 
    class Renderer
    {
    public:
		Renderer(HWND hWnd, uint32_t width, uint32_t height);
        
        void Draw();
        void Update();

		void CalcFrameState();
		void Tick();
		void Stop();
		void Start();

        void OnMouseDown(WPARAM btnState, int x, int y);
		void OnMouseUp(WPARAM btnState, int x, int y);
		void OnMouseMove(WPARAM btnState, int x, int y);
		void OnKeyboardInput();
        void OnResize(uint32_t width, uint32_t height, bool init = false);

		void SetPaused(bool state);
		bool IsPaused();
		void SetMaximized(bool state);
		bool IsZoomed();
		void SetMinimized(bool state);
		bool IsIconic();
		void SetResizing(bool state);
		bool IsResizing();

        void LoadModel(std::wstring_view path, std::wstring_view textureDir, std::wstring_view modelName, DirectX::XMMATRIX world, bool isSkinned);
        void UnloadModel(std::wstring_view modelName);
        std::vector<std::wstring_view> GetAnimationNames(std::wstring_view modelName);
        void SetAnimation(std::wstring_view modelName, std::wstring_view animationName);
        std::vector<std::wstring_view> GetModelNames();
    protected:
		void InitDebug();
		void InitDxgiFactory();
		void InitDevice();
		void InitFence();
		void InitCommandQueue();
		void InitCommandAllocatorPool();
		void InitGraphicsCommandList();
        void InitPipelineStates();

		void InitHeapManager();
		void InitDescriptorManager();
		void InitTextureManager();
		void InitTimer();
		void InitCamera();
        void InitScene();
		void InitConstants();

		void InitRenderPass();
		void InitDisplay();
        void InitFrame();
        void InitMainLight();
        void InitNormal();
        void InitSsao();
		void InitToneMapping();
        void InitTaa();
		
		float AspectRatio();
		void FlushCommandQueue();
        void ReleaseIntermediateBuffers();

    protected:
		uint32_t mClientWidth = 0;
		uint32_t mClientHeight = 0;
		std::unique_ptr<Camera> mCamera;
		std::unique_ptr<Timer> mTimer;

		HWND mhWnd;
		std::wstring mMainWndCaption = L"Carol";
		D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;

		DirectX::XMINT2 mLastMousePos = { 0,0 };

		bool mPaused = false;
		bool mMaximized = false;
		bool mMinimized = false;
		bool mResizing = false;

        std::unique_ptr<DisplayPass> mDisplayPass;
		std::unique_ptr<FramePass> mFramePass;
        std::unique_ptr<NormalPass> mNormalPass;
        std::unique_ptr<CascadedShadowPass> mMainLightShadowPass;
        std::unique_ptr<SsaoPass> mSsaoPass;
        std::unique_ptr<TaaPass> mTaaPass;
		std::unique_ptr<ToneMappingPass> mToneMappingPass;

		std::unique_ptr<FrameConstants> mFrameConstants;
        std::unique_ptr<FastConstantBufferAllocator> mFrameCBAllocator;
		D3D12_GPU_VIRTUAL_ADDRESS mFrameCBAddr;
    };

}

