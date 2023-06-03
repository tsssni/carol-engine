#pragma once
#include <scene/light.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>
#include <unordered_map>
#include <string>
#include <string_view>

#define MAX_MAIN_LIGHT_SPLIT_LEVEL 8
#define MAX_POINT_LIGHTS 64
#define MAX_SPOT_LIGHTS 64

namespace Carol
{
	class FastConstantBufferAllocator;
	class Camera;
	class Timer;
	class Scene;
	class CullPass;
	class DisplayPass;
	class GeometryPass;
	class OitppllPass;
    class ShadePass;
    class CascadedShadowPass;
    class SsaoPass;
    class SsgiPass;
    class TaaPass;
	class ToneMappingPass;
	class UtilsPass;

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
		
		DirectX::XMFLOAT4X4 VeloViewProj;
		DirectX::XMFLOAT4X4 HistViewProj;

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
		float GaussBlurWeights[12];
		float OcclusionRadius = 0.5f;
		float OcclusionFadeStart = 0.2f;
		float OcclusionFadeEnd = 1.0f;
		float SurfaceEplison = 0.2f;

		uint32_t NumMainLights;
		DirectX::XMFLOAT3 AmbientColor;
		Light MainLights[MAX_MAIN_LIGHT_SPLIT_LEVEL];
		float MainLightSplitZ[MAX_MAIN_LIGHT_SPLIT_LEVEL];
		uint32_t MainLightShadowMapIdx[MAX_MAIN_LIGHT_SPLIT_LEVEL] = { 0 };

		uint32_t NumPointLights = 0;
		DirectX::XMFLOAT3 FramePad2;
		Light PointLights[MAX_POINT_LIGHTS];

		uint32_t NumSpotLights = 0;
		DirectX::XMFLOAT3 FramePad3;
		Light SpotLights[MAX_POINT_LIGHTS];

		// Cull
		uint32_t FrameHiZMapIdx = 0;

		// Display
		uint32_t RWFrameMapIdx = 0;
		uint32_t RWHistMapIdx = 0;
		uint32_t DepthStencilMapIdx = 0;
		uint32_t SkyBoxIdx = 0;

		// G-Buffer
		uint32_t DiffuseRougnessMapIdx = 0;
		uint32_t EmissiveMetallicMapIdx = 0;
		uint32_t NormalMapIdx = 0;
		uint32_t VelocityMapIdx = 0;

		// OITPPLL
		uint32_t RWOitppllBufferIdx = 0;
		uint32_t RWOitppllStartOffsetBufferIdx = 0;
		uint32_t RWOitppllCounterIdx = 0;
		uint32_t OitppllBufferIdx = 0;
		uint32_t OitppllStartOffsetBufferIdx = 0;

		// SSAO
		uint32_t RWAmbientMapIdx = 0;
		uint32_t AmbientMapIdx = 0;

		// SSGI
		uint32_t RWSceneColorIdx = 0;
		uint32_t SceneColorIdx = 0;
		uint32_t RWSsgiMapIdx = 0;
		uint32_t SsgiMapIdx = 0;

		// Utils
		uint32_t RandVecMapIdx = 0;

		DirectX::XMFLOAT2 FramePad4;
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

        void LoadModel(std::string_view path, std::string_view textureDir, std::string_view modelName, DirectX::XMMATRIX world, bool isSkinned);
        void UnloadModel(std::string_view modelName);
        std::vector<std::string_view> GetAnimationNames(std::string_view modelName);
        void SetAnimation(std::string_view modelName, std::string_view animationName);
        std::vector<std::string_view> GetModelNames();
    protected:
		void InitDebug();
		void InitDxgiFactory();
		void InitDevice();
		void InitFence();
		void InitCommandQueue();
		void InitCommandAllocatorPool();
		void InitGraphicsCommandList();
		void InitRootSignature();
		void InitCommandSignature();

		void InitHeapManager();
		void InitDescriptorManager();
		void InitShaderManager();
		void InitTextureManager();
        void InitModelManager();
		void InitTimer();
		void InitCamera();

		void InitConstants();
		void InitSkyBox();
		void InitRandomVectors();
		void InitGaussWeights();

		void InitCullPass();
		void InitDisplayPass();
		void InitGeometryPass();
		void InitOitppllPass();
		void InitShadePass();
        void InitMainLightShadowPass();
        void InitSsaoPass();
		void InitSsgiPass();
		void InitToneMappingPass();
        void InitTaaPass();
		void InitUtilsPass();
		
		float AspectRatio();
		void FlushCommandQueue();

    protected:
		uint32_t mClientWidth = 0;
		uint32_t mClientHeight = 0;
		std::unique_ptr<Camera> mCamera;
		std::unique_ptr<Timer> mTimer;

		HWND mhWnd;
		std::string mMainWndCaption = "Carol";
		D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;

		DirectX::XMINT2 mLastMousePos = { 0,0 };

		bool mPaused = false;
		bool mMaximized = false;
		bool mMinimized = false;
		bool mResizing = false;

		std::unique_ptr<CullPass> mCullPass;
        std::unique_ptr<DisplayPass> mDisplayPass;
		std::unique_ptr<GeometryPass> mGeometryPass;
		std::unique_ptr<OitppllPass> mOitppllPass;
		std::unique_ptr<ShadePass> mShadePass;
        std::unique_ptr<CascadedShadowPass> mMainLightShadowPass;
        std::unique_ptr<SsaoPass> mSsaoPass;
		std::unique_ptr<SsgiPass> mSsgiPass;
        std::unique_ptr<TaaPass> mTaaPass;
		std::unique_ptr<ToneMappingPass> mToneMappingPass;
		std::unique_ptr<UtilsPass> mUtilsPass;

		std::unique_ptr<FrameConstants> mFrameConstants;
        std::unique_ptr<FastConstantBufferAllocator> mFrameCBAllocator;
		D3D12_GPU_VIRTUAL_ADDRESS mFrameCBAddr;
    };

}

