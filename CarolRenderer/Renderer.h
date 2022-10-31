#pragma once
#include "FrameResources.h"
#include "DirectX/DescriptorHeap.h"
#include "DirectX/Shader.h"
#include "BaseRenderer.h"
#include "Resource/Model.h"
#include "Resource/AssimpModel.h"
#include "Resource/Texture.h"
#include "Resource/Material.h"
#include "Ambient Occlusion/Ssao.h"
#include "Anti-Aliasing/Taa.h"
#include <memory>
#include <unordered_map>
#include <string>

using std::unique_ptr;
using std::unordered_map;
using std::wstring;

namespace Carol
{
    class Renderer :public BaseRenderer
    {
    public:
		void InitRenderer(HWND hWnd, uint32_t width, uint32_t height)override;
        
        virtual void Update()override;
        virtual void Draw()override;

        virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
		virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
		virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
		virtual void OnKeyboardInput()override;
        virtual void OnResize(uint32_t width, uint32_t height)override;
    protected:
        void InitRootSignature();
        void InitSsaoRootSignature();
        static vector<CD3DX12_STATIC_SAMPLER_DESC>& GetDefaultStaticSamplers();

        void InitRtvDsvDescriptorHeaps()override;
        void InitCbvSrvUavDescriptorHeaps()override;
        void InitTexturesDescriptors(uint32_t startIndex);
        void InitMaterialBufferDescriptor(uint32_t startIndex);

        void InitModels()override;
        void InitMaterials(vector<AssimpMaterial>& mats);
        void InitMaterialBuffer();
        void InitTextures(unordered_map<wstring, uint32_t>& texIndexMap, wstring dirPath);

        void InitShaders()override;
        void InitRenderItems()override;
        void InitFrameResources()override;
        void InitPSOs()override;

        void DrawNormalsAndDepth();
        void DrawTaa();
        void DrawTaaCurrFrameMap();
        void DrawTaaVelocityMap();
        void DrawTaaOutput();
        void DrawRenderItems(vector<RenderItem*>& ritems);

        void UpdateCamera();
        void UpdatePassCBs();
        void UpdateObjCBs();
        void UpdateSkinnedCBs();
        void UpdateSsaoCB();

    protected:
        ComPtr<ID3D12RootSignature> mRootSignature;
        ComPtr<ID3D12RootSignature> mSsaoRootSignature;

        unordered_map<wstring, unique_ptr<Model>> mModels;
        unordered_map<wstring, unique_ptr<SkinnedData>> mSkinnedInfo;
        unordered_map<wstring, unique_ptr<SkinnedModelInfo>> mSkinnedInstances;
        
        vector<unique_ptr<RenderItem>> mRenderItems;
        vector<RenderItem*> mOpaqueItems;

        unordered_map<wstring, unique_ptr<Texture>> mTextures;
        unordered_map<wstring, uint32_t> mTexturesIndex;
        D3D12_GPU_DESCRIPTOR_HANDLE mTexturesSrv;

        vector<MaterialData> mMaterialData;
        unique_ptr<DefaultBuffer> mMaterialBuffer;
        D3D12_GPU_DESCRIPTOR_HANDLE mMaterialBufferSrv;

        vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
        vector<D3D12_INPUT_ELEMENT_DESC> mNullInputLayout;

        unordered_map<wstring, unique_ptr<Shader>> mShaders;
        unordered_map<wstring, ComPtr<ID3D12PipelineState>> mPSOs;

        vector<unique_ptr<FrameResource>> mFrameResources;
        int mCurrFrameResourceIndex = 0;
        FrameResource* mCurrFrameResource;

        unique_ptr<SsaoManager> mSsao;
        unique_ptr<TaaManager> mTaa;
    };

}

