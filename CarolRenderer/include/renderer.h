#pragma once
#define MAX_LIGHTS 16

#include <base_renderer.h>
#include <manager/light.h>
#include <memory>
#include <unordered_map>
#include <string>

namespace Carol
{
    class GlobalResources;
    class Heap;
    class CircularHeap;
    class DescriptorAllocator;
    class Shader;
    class SsaoManager;
    class TaaManager;
    class LightManager;
    class OitppllManager;
    class MeshManager;
    class AssimpModel;

    class PassConstants
	{
	public:
		DirectX::XMFLOAT4X4 View;
		DirectX::XMFLOAT4X4 InvView;
		DirectX::XMFLOAT4X4 Proj;
		DirectX::XMFLOAT4X4 InvProj;
		DirectX::XMFLOAT4X4 ViewProj;
		DirectX::XMFLOAT4X4 InvViewProj;

        DirectX::XMFLOAT4X4 ViewProjTex;
        
		DirectX::XMFLOAT4X4 HistViewProj;
        DirectX::XMFLOAT4X4 JitteredViewProj;

		DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
		float CbPad1 = 0.0f;
		DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
		DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
		float NearZ = 0.0f;
		float FarZ = 0.0f;
		float CbPad2;
		float CbPad3;

		LightData Lights[MAX_LIGHTS];
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
        virtual void OnResize(uint32_t width, uint32_t height)override;

        void LoadModel(std::wstring path, std::wstring textureDir, std::wstring modelName, DirectX::XMMATRIX world, bool isSkinned, bool isTransparent);
        void UnloadModel(std::wstring modelName);
        std::vector<std::wstring> GetAnimationNames(std::wstring modelName);
        void SetAnimation(std::wstring modelName, std::wstring animationName);
        std::vector<std::wstring> GetModelNames();
    protected:
        void InitFrameAllocators();
        void InitRootSignature();
        void InitConstants();
        void InitShaders();
        void InitPSOs();
        void InitSsao();
        void InitTaa();
        void InitMainLight();
        void InitOitppll();
        void InitMeshes();
        void InitGpuDescriptors();

        void ReleaseIntermediateBuffers();

        void UpdateCamera();
        void UpdatePassCB();
        void UpdateMeshes();
        void UpdateAnimations();

    protected:
        Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;

        std::unordered_map<std::wstring, std::unique_ptr<AssimpModel>> mModels;
        std::unique_ptr<AssimpModel> mSkyBox;
        
        std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
        std::vector<D3D12_INPUT_ELEMENT_DESC> mNullInputLayout;
        D3D12_GRAPHICS_PIPELINE_STATE_DESC mBasePsoDesc;

        std::vector<MeshManager*> mOpaqueStaticMeshes;
        std::vector<MeshManager*> mTransparentStaticMeshes;
        std::vector<MeshManager*> mOpaqueSkinnedMeshes;
        std::vector<MeshManager*> mTransparentSkinnedMeshes;

        std::unordered_map<std::wstring, std::unique_ptr<Shader>> mShaders;
        std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

        uint32_t mNumFrame = 3;
        uint32_t mCurrFrame = 0;
        std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> mFrameAllocator;
        std::vector<uint32_t> mGpuFence = { 0,0,0 };

        std::unique_ptr<PassConstants> mPassConstants;
        std::unique_ptr<HeapAllocInfo> mPassCBAllocInfo;
        std::unique_ptr<CircularHeap> mPassCBHeap;

        std::unique_ptr<SsaoManager> mSsao;
        std::unique_ptr<TaaManager> mTaa;
        std::unique_ptr<LightManager> mMainLight;
        std::unique_ptr<OitppllManager> mOitppll;
    };

}

