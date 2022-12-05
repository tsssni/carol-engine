#pragma once
#define MAX_LIGHTS 16

#include "BaseRenderer.h"
#include "Manager/Light/Light.h"
#include <memory>
#include <unordered_map>
#include <string>

using std::unique_ptr;
using std::unordered_map;
using std::wstring;

namespace Carol
{
    class RenderData;
    class Heap;
    class CircularHeap;
    class DescriptorAllocator;
    class Shader;
    class SsaoManager;
    class TaaManager;
    class LightManager;
    class OitppllManager;
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

        void LoadModel(wstring path, wstring textureDir, wstring modelName, DirectX::XMMATRIX world, bool isSkinned, bool isTransparent);
        void UnloadModel(wstring modelName);
        vector<wstring> GetAnimationNames(wstring modelName);
        void SetAnimation(wstring modelName, wstring animationName);
        vector<wstring> GetModelNames();
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
        ComPtr<ID3D12RootSignature> mRootSignature;

        unique_ptr<Heap> mDefaultBuffersHeap;
        unique_ptr<Heap> mUploadBuffersHeap;
        unique_ptr<Heap> mReadbackBuffersHeap;
        unique_ptr<Heap> mSrvTexturesHeap;
        unique_ptr<Heap> mRtvDsvTexturesHeap;

        unordered_map<wstring, unique_ptr<AssimpModel>> mModels;
        unique_ptr<AssimpModel> mSkyBox;
        
        vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
        vector<D3D12_INPUT_ELEMENT_DESC> mNullInputLayout;
        D3D12_GRAPHICS_PIPELINE_STATE_DESC mBasePsoDesc;

        vector<MeshManager*> mOpaqueStaticMeshes;
        vector<MeshManager*> mTransparentStaticMeshes;
        vector<MeshManager*> mOpaqueSkinnedMeshes;
        vector<MeshManager*> mTransparentSkinnedMeshes;

        unordered_map<wstring, unique_ptr<Shader>> mShaders;
        unordered_map<wstring, ComPtr<ID3D12PipelineState>> mPSOs;

        uint32_t mNumFrame = 3;
        uint32_t mCurrFrame = 0;
        vector<ComPtr<ID3D12CommandAllocator>> mFrameAllocator;
        vector<uint32_t> mGpuFence = { 0,0,0 };

        unique_ptr<PassConstants> mPassConstants;
        unique_ptr<HeapAllocInfo> mPassCBAllocInfo;
        unique_ptr<CircularHeap> mPassCBHeap;

        unique_ptr<SsaoManager> mSsao;
        unique_ptr<TaaManager> mTaa;
        unique_ptr<LightManager> mMainLight;
        unique_ptr<OitppllManager> mOitppll;
    };

}

