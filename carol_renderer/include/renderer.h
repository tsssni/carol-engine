#pragma once

#include <base_renderer.h>
#include <memory>
#include <unordered_map>
#include <string>

namespace Carol
{
    class FramePass;
    class SsaoPass;
    class NormalPass;
    class TaaPass;
    class ShadowPass;
    class OitppllPass;
    class MeshesPass;
    class TextureManager;
 
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
        void InitPSOs();
        void InitFrame();
        void InitSsao();
        void InitNormal();
        void InitTaa();
        void InitMainLight();
        void InitOitppll();
        void InitMeshes();

        void ReleaseIntermediateBuffers();
        void CopyDescriptors();
        void SetTextures();

    protected:
        std::unique_ptr<FramePass> mFrame;
        std::unique_ptr<SsaoPass> mSsao;
        std::unique_ptr<NormalPass> mNormal;
        std::unique_ptr<TaaPass> mTaa;
        std::unique_ptr<ShadowPass> mMainLight;
        std::unique_ptr<OitppllPass> mOitppll;
        std::unique_ptr<MeshesPass> mMeshes;
        std::unique_ptr<TextureManager> mTexManager;
    };

}

