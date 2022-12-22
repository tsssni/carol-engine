#pragma once

#include <base_renderer.h>
#include <memory>
#include <unordered_map>
#include <string>

namespace Carol
{
    class GlobalResources;
    class FrameConstants;
    class Heap;
    class HeapAllocInfo;
    class CircularHeap;
    class DescriptorAllocator;
    class DescriptorAllocInfo;
    class Shader;
    class SsaoPass;
    class TaaPass;
    class ShadowPass;
    class OitppllPass;
    class MeshPass;
    class AssimpModel;
    class Texture;
    class FrameConstants;
 
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
        void InitConstants();
        void InitShaders();
        void InitPSOs();
        void InitSsao();
        void InitTaa();
        void InitMainLight();
        void InitOitppll();
        void InitMeshes();

        void ReleaseIntermediateBuffers();

        void UpdateCamera();
        void UpdateFrameCB();
        void UpdateMeshes();
        void UpdateAnimations();

    protected:
        std::unordered_map<std::wstring, std::unique_ptr<AssimpModel>> mModels;
        std::unique_ptr<AssimpModel> mSkyBox;
        std::unique_ptr<Texture> mSkyBoxTex;
        std::unique_ptr<DescriptorAllocInfo> mSkyBoxAllocInfo;
        
        std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
        std::vector<D3D12_INPUT_ELEMENT_DESC> mNullInputLayout;
        D3D12_GRAPHICS_PIPELINE_STATE_DESC mBasePsoDesc;

        std::vector<MeshPass*> mOpaqueStaticMeshes;
        std::vector<MeshPass*> mTransparentStaticMeshes;
        std::vector<MeshPass*> mOpaqueSkinnedMeshes;
        std::vector<MeshPass*> mTransparentSkinnedMeshes;

        std::unordered_map<std::wstring, std::unique_ptr<Shader>> mShaders;
        std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

        uint32_t mNumFrame = 3;
        uint32_t mCurrFrame = 0;
        std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> mFrameAllocator;
        std::vector<uint32_t> mGpuFence = { 0,0,0 };

        std::unique_ptr<FrameConstants> mFrameConstants;
        std::unique_ptr<HeapAllocInfo> mFrameCBAllocInfo;
        std::unique_ptr<CircularHeap> mFrameCBHeap;

        std::unique_ptr<SsaoPass> mSsao;
        std::unique_ptr<TaaPass> mTaa;
        std::unique_ptr<ShadowPass> mMainLight;
        std::unique_ptr<OitppllPass> mOitppll;
    };

}

