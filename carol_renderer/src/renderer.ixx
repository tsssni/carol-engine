export module carol.renderer;
export import carol.renderer.dx12;
export import carol.renderer.render_pass;
export import carol.renderer.scene;
export import carol.renderer.utils;
import <dxgi1_6.h>;
import <DirectXMath.h>;
import <DirectXColors.h>;
import <wrl/client.h>;
import <memory>;
import <vector>;
import <unordered_map>;
import <string>;
import <string_view>;

#define MAX_LIGHTS 16
#define MAIN_LIGHT_SPLIT_LEVEL 5

namespace Carol
{
    using Microsoft::WRL::ComPtr;
    using std::make_unique;
    using std::to_wstring;
    using std::unique_ptr;
    using std::unordered_map;
    using std::vector;
    using std::wstring;
    using std::wstring_view;
    using namespace DirectX;

    class FrameConstants
    {
    public:
        // Transformation
        XMFLOAT4X4 View;
        XMFLOAT4X4 InvView;
        XMFLOAT4X4 Proj;
        XMFLOAT4X4 InvProj;
        XMFLOAT4X4 ViewProj;
        XMFLOAT4X4 InvViewProj;
        XMFLOAT4X4 ProjTex;
        XMFLOAT4X4 ViewProjTex;
        XMFLOAT4X4 HistViewProj;
        XMFLOAT4X4 JitteredViewProj;

        // View
        XMFLOAT3 EyePosW = {0.0f, 0.0f, 0.0f};
        float FramePad0 = 0.0f;
        XMFLOAT2 RenderTargetSize = {0.0f, 0.0f};
        XMFLOAT2 InvRenderTargetSize = {0.0f, 0.0f};
        float NearZ = 0.0f;
        float FarZ = 0.0f;
        XMFLOAT2 FramePad1;

        // Ssao
        XMFLOAT4 OffsetVectors[14];
        XMFLOAT4 BlurWeights[3];
        float OcclusionRadius = 0.5f;
        float OcclusionFadeStart = 0.2f;
        float OcclusionFadeEnd = 1.0f;
        float SurfaceEplison = 0.05f;

        Light Lights[MAX_LIGHTS];
        float MainLightSplitZ[MAIN_LIGHT_SPLIT_LEVEL + 1];
		float FramePad2[2];

        uint32_t MeshCBIdx = 0;
        uint32_t CommandBufferIdx = 0;
        uint32_t InstanceFrustumCulledMarkIdx = 0;
        uint32_t InstanceOcclusionPassedMarkIdx = 0;

        uint32_t FrameMapIdx = 0;
        uint32_t DepthStencilMapIdx = 0;
        uint32_t NormalMapIdx = 0;
        float FramePad3;

        // Main Light
        uint32_t MainLightShadowMapIdx[MAIN_LIGHT_SPLIT_LEVEL] = { 0 };
		float FramePad4[3];

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

        float FramePad5[2];
    };

    class BaseRenderer
    {
    public:
        BaseRenderer(HWND hWnd)
        	:mhWnd(hWnd)
        {
            InitTimer();
            InitCamera();

        #if defined _DEBUG
            InitDebug();
        #endif
            InitDxgiFactory();
            InitDevice();
            InitFence();

            InitCommandAllocator();
            InitCommandList();
            InitCommandQueue();

            InitRootSignature();
            InitCommandSignature();

            InitHeapManager();
            InitDescriptorManager();
            InitTextureManager();
            InitDisplay();
        }

        BaseRenderer(const BaseRenderer &) = delete;
        BaseRenderer(BaseRenderer &&) = delete;
        BaseRenderer &operator=(const BaseRenderer &) = delete;

        ~BaseRenderer()
        {
            FlushCommandQueue();

            gTextureManager.reset();
            gTextureManager = nullptr;

            gDescriptorManager.reset();
            gDescriptorManager = nullptr;

            gHeapManager.reset();
            gHeapManager = nullptr;
        }

        virtual void CalcFrameState()
        {
            static int frameCnt = 0;
            static float timeElapsed = 0.0f;

            frameCnt++;

            // Compute averages over one second period.
            if ((mTimer->TotalTime() - timeElapsed) >= 1.0f)
            {
                float fps = (float)frameCnt; // fps = frameCnt / 1
                float mspf = 1000.0f / fps;

                wstring fpsStr = to_wstring(fps);
                wstring mspfStr = to_wstring(mspf);

                wstring windowText = mMainWndCaption +
                    L"    fps: " + fpsStr +
                    L"   mspf: " + mspfStr;

                SetWindowTextW(mhWnd, windowText.c_str());

                // Reset for next average.
                frameCnt = 0;
                timeElapsed += 1.0f;
            }
        }

        virtual void Update() = 0;

        virtual void Draw() = 0;

        virtual void Tick()
        {
            mTimer->Tick();
        }

        virtual void Stop()
        {
            mTimer->Stop();
        }

        virtual void Start()
        {
            mTimer->Start();
        }

        virtual void OnMouseDown(WPARAM btnState, int x, int y) = 0;

        virtual void OnMouseUp(WPARAM btnState, int x, int y) = 0;

        virtual void OnMouseMove(WPARAM btnState, int x, int y) = 0;

        virtual void OnKeyboardInput() = 0;

        virtual void OnResize(uint32_t width, uint32_t height, bool init = false)
        {
            if (mClientWidth == width && mClientHeight == height)
            {
                return;
            }

            mClientWidth = width;
            mClientHeight = height;

            mTimer->Start();
            dynamic_cast<PerspectiveCamera*>(mCamera.get())->SetLens(0.25f * DirectX::XM_PI, AspectRatio(), 1.0f, 1000.0f);
            gDisplayPass->OnResize(mClientWidth, mClientHeight);
        }

        virtual void SetPaused(bool state)
        {
            mPaused = state;
        }

        virtual bool IsPaused()
        {
            return mPaused;
        }

        virtual void SetMaximized(bool state)
        {
            mMaximized = state;
        }

        virtual bool IsZoomed()
        {
            return mMaximized;
        }

        virtual void SetMinimized(bool state)
        {
            mMinimized = state;
        }

        virtual bool IsIconic()
        {
            return mMinimized;
        }

        virtual void SetResizing(bool state)
        {
            mResizing = state;
        }

        virtual bool IsResizing()
        {
            return mResizing;
        }

    protected:
        float AspectRatio()
        {
            return 1.0f * mClientWidth / mClientHeight;
        }

        virtual void InitTimer()
        {
            mTimer = make_unique<Timer>();
            mTimer->Reset();
        }

        virtual void InitCamera()
        {
            mCamera = make_unique<PerspectiveCamera>();
            mCamera->SetPosition(0.0f, 5.0f, -5.0f);
        }

        virtual void InitDebug()
        {
            ComPtr<ID3D12Debug5> debugLayer;
            ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(debugLayer.GetAddressOf())));
            debugLayer->EnableDebugLayer();
            debugLayer->SetEnableAutoName(true);

            mDebugLayer = debugLayer;
        }

        virtual void InitDxgiFactory()
        {
            ComPtr<IDXGIFactory4> dxgiFactory;
            ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory.GetAddressOf())));
            mDxgiFactory = dxgiFactory;
        }

        virtual void InitDevice()
        {
            ComPtr<ID3D12Device2> device;
            ThrowIfFailed(D3D12CreateDevice(device.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(device.GetAddressOf())));
            
            gDevice = device;
        }

        virtual void InitFence()
        {
            ComPtr<ID3D12Fence> fence;
            ThrowIfFailed(gDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())));
            mFence = fence;
        }

        virtual void InitCommandQueue()
        {
            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

            ThrowIfFailed(gDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(gCommandQueue.GetAddressOf())));
        }

        virtual void InitCommandAllocator()
        {
            ThrowIfFailed(gDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mInitCommandAllocator.GetAddressOf())));

            gNumFrame = 3;
            gCurrFrame = 0;

            mFrameAllocator.resize(gNumFrame);
            for (int i = 0; i < gNumFrame; ++i)
            {
                ThrowIfFailed(gDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mFrameAllocator[i].GetAddressOf())));
            }
        }

        virtual void InitCommandList()
        {
            ComPtr<ID3D12GraphicsCommandList6> cmdList;

            ThrowIfFailed(gDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mInitCommandAllocator.Get(), nullptr, IID_PPV_ARGS(cmdList.GetAddressOf())));
            cmdList->Close();

            gCommandList = cmdList;
        }

        virtual void InitCommandSignature()
        {
            D3D12_INDIRECT_ARGUMENT_DESC argDesc[3];

            argDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
            argDesc[0].ConstantBufferView.RootParameterIndex = MESH_CB;
            
            argDesc[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
            argDesc[1].ConstantBufferView.RootParameterIndex = SKINNED_CB;

            argDesc[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;
            
            D3D12_COMMAND_SIGNATURE_DESC cmdSigDesc;
            cmdSigDesc.pArgumentDescs = argDesc;
            cmdSigDesc.NumArgumentDescs = _countof(argDesc);
            cmdSigDesc.ByteStride = sizeof(IndirectCommand);
            cmdSigDesc.NodeMask = 0;

            ThrowIfFailed(gDevice->CreateCommandSignature(&cmdSigDesc, gRootSignature->Get(), IID_PPV_ARGS(gCommandSignature.GetAddressOf())));
        }

        virtual void InitRootSignature()
        {
            Shader::InitCompiler();
            gRootSignature = make_unique<RootSignature>();
        }

        virtual void InitHeapManager()
        {
            gHeapManager = make_unique<HeapManager>(1 << 27);
            StructuredBuffer::InitCounterResetBuffer(gHeapManager->GetUploadBuffersHeap());
        }

        virtual void InitDescriptorManager()
        {
            gDescriptorManager = make_unique<DescriptorManager>();
        }

        virtual void InitTextureManager()
        {
            gTextureManager = make_unique<TextureManager>();
        }

        virtual void InitDisplay()
        {
            gDisplayPass = make_unique<DisplayPass>(mhWnd, mDxgiFactory.Get(), 2);
        }

        virtual void FlushCommandQueue()
        {
            ++mCpuFence;
            ThrowIfFailed(gCommandQueue->Signal(mFence.Get(), mCpuFence));
            
            if (mFence->GetCompletedValue() < mCpuFence)
            {
                auto eventHandle = CreateEventExW(nullptr, LPCWSTR(nullptr), 0, EVENT_ALL_ACCESS);
                ThrowIfFailed(mFence->SetEventOnCompletion(mCpuFence, eventHandle));

                WaitForSingleObject(eventHandle, INFINITE);
                CloseHandle(eventHandle);
            }
        }

    protected:
        ComPtr<ID3D12Debug> mDebugLayer;
        ComPtr<IDXGIFactory> mDxgiFactory;
        ComPtr<ID3D12Fence> mFence;
        uint32_t mCpuFence = 0;

        ComPtr<ID3D12CommandAllocator> mInitCommandAllocator;
        vector<ComPtr<ID3D12CommandAllocator>> mFrameAllocator;
        vector<uint32_t> mGpuFence = {0, 0, 0};

        uint32_t mClientWidth = 0;
        uint32_t mClientHeight = 0;
        unique_ptr<Camera> mCamera;
        unique_ptr<Timer> mTimer;

        HWND mhWnd;
        wstring mMainWndCaption = L"Carol";
        D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;

        XMINT2 mLastMousePos = {0, 0};

        bool mPaused = false;
        bool mMaximized = false;
        bool mMinimized = false;
        bool mResizing = false;
    };

    export class Renderer : public BaseRenderer
    {
    public:
        Renderer(HWND hWnd, uint32_t width, uint32_t height)
        	:BaseRenderer(hWnd)
        {
            ThrowIfFailed(mInitCommandAllocator->Reset());
            ThrowIfFailed(gCommandList->Reset(mInitCommandAllocator.Get(), nullptr));
            
            InitConstants();
            InitPipelineStates();
            InitFrame();
            InitNormal();
            InitMainLight();
            InitSsao();
            InitTaa();
            InitScene();
            OnResize(width, height, true);
            ReleaseIntermediateBuffers();
        }

        ~Renderer()
        {
            gFramePass.reset();
            gFramePass = nullptr;

            gDisplayPass.reset();
            gDisplayPass = nullptr;

            gScene.reset();
            gScene = nullptr;
        }

        virtual void Draw() override
        {	
            ID3D12DescriptorHeap* descriptorHeaps[] = {gDescriptorManager->GetResourceDescriptorHeap()};
            gCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

            gCommandList->SetGraphicsRootSignature(gRootSignature->Get());
            gCommandList->SetComputeRootSignature(gRootSignature->Get());

            gCommandList->SetGraphicsRootConstantBufferView(FRAME_CB, mFrameCBAddr);
            gCommandList->SetComputeRootConstantBufferView(FRAME_CB, mFrameCBAddr);

            mMainLightShadowPass->Draw();
            gFramePass->Cull();
            mNormalPass->Draw();
            mSsaoPass->Draw();
            gFramePass->Draw();
            mTaaPass->Draw();
            
            ThrowIfFailed(gCommandList->Close());
            vector<ID3D12CommandList*> cmdLists{ gCommandList.Get()};
            gCommandQueue->ExecuteCommandLists(1, cmdLists.data());

            gDisplayPass->Present();
            mGpuFence[gCurrFrame] = ++mCpuFence;
            ThrowIfFailed(gCommandQueue->Signal(mFence.Get(), mCpuFence));

            gCurrFrame = (gCurrFrame + 1) % gNumFrame;
        }

        virtual void Update() override
        {
            OnKeyboardInput();

            if (mGpuFence[gCurrFrame] != 0 && mFence->GetCompletedValue() < mGpuFence[gCurrFrame])
            {
                HANDLE eventHandle = CreateEventExW(nullptr, LPCWSTR(nullptr), 0, EVENT_ALL_ACCESS);
                ThrowIfFailed(mFence->SetEventOnCompletion(mGpuFence[gCurrFrame], eventHandle));
                WaitForSingleObject(eventHandle, INFINITE);
                CloseHandle(eventHandle);
            }

            ThrowIfFailed(mFrameAllocator[gCurrFrame]->Reset());
            ThrowIfFailed(gCommandList->Reset(mFrameAllocator[gCurrFrame].Get(), nullptr));
            
            gDescriptorManager->DelayedDelete();
            gHeapManager->DelayedDelete();

            mCamera->UpdateViewMatrix();
            mMainLightShadowPass->Update(dynamic_cast<PerspectiveCamera*>(mCamera.get()), 0.85f);
            UpdateFrameCB();

            gScene->Update(mCamera.get(), mTimer.get());
            gFramePass->Update();
        }

        virtual void OnMouseDown(WPARAM btnState, int x, int y) override
        {
            mLastMousePos.x = x;
            mLastMousePos.y = y;

            SetCapture(mhWnd);
        }

        virtual void OnMouseUp(WPARAM btnState, int x, int y) override
        {
            ReleaseCapture();
        }

        virtual void OnMouseMove(WPARAM btnState, int x, int y) override
        {
            if ((btnState & MK_LBUTTON) != 0)
            {
                // Make each pixel correspond to a quarter of a degree.
                float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
                float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

                mCamera->RotateY(dx);
                mCamera->Pitch(dy);
            }

            mLastMousePos.x = x;
            mLastMousePos.y = y;
        }

        virtual void OnKeyboardInput() override
        {
            const float dt = mTimer->DeltaTime();

            if (GetAsyncKeyState('W') & 0x8000)
                mCamera->Walk(10.0f * dt);

            if (GetAsyncKeyState('S') & 0x8000)
                mCamera->Walk(-10.0f * dt);

            if (GetAsyncKeyState('A') & 0x8000)
                mCamera->Strafe(-10.0f * dt);

            if (GetAsyncKeyState('D') & 0x8000)
                mCamera->Strafe(10.0f * dt);
        }

        virtual void OnResize(uint32_t width, uint32_t height, bool init = false) override
        {
            if (!init)
            {
                ThrowIfFailed(mInitCommandAllocator->Reset());
                ThrowIfFailed(gCommandList->Reset(mInitCommandAllocator.Get(), nullptr));
            }

            BaseRenderer::OnResize(width, height);
            gFramePass->OnResize(width, height);
            mNormalPass->OnResize(width, height);
            mSsaoPass->OnResize(width, height);
            mTaaPass->OnResize(width, height);

            mFrameConstants->MeshCBIdx = gScene->GetMeshCBIdx();
            mFrameConstants->CommandBufferIdx = gScene->GetCommandBufferIdx();
            mFrameConstants->InstanceFrustumCulledMarkIdx = gScene->GetInstanceFrustumCulledMarkBufferIdx();
            mFrameConstants->InstanceOcclusionPassedMarkIdx = gScene->GetInstanceOcclusionPassedMarkBufferIdx();
            
            mFrameConstants->FrameMapIdx = gFramePass->GetFrameSrvIdx();
            mFrameConstants->DepthStencilMapIdx = gFramePass->GetDepthStencilSrvIdx();
            mFrameConstants->NormalMapIdx = mNormalPass->GetNormalSrvIdx();
            
            // Main light
            for (int i = 0; i < mMainLightShadowPass->GetSplitLevel(); ++i)
            {
                mFrameConstants->MainLightShadowMapIdx[i] = mMainLightShadowPass->GetShadowSrvIdx(i);
            }

            //OITPPLL
            mFrameConstants->OitBufferWIdx = gFramePass->GetPpllUavIdx();
            mFrameConstants->OitOffsetBufferWIdx = gFramePass->GetOffsetUavIdx();
            mFrameConstants->OitCounterIdx = gFramePass->GetCounterUavIdx();
            mFrameConstants->OitBufferRIdx = gFramePass->GetPpllSrvIdx();
            mFrameConstants->OitOffsetBufferRIdx = gFramePass->GetOffsetSrvIdx();
            
            // SSAO
            mFrameConstants->RandVecMapIdx = mSsaoPass->GetRandVecSrvIdx();
            mFrameConstants->AmbientMapWIdx = mSsaoPass->GetSsaoUavIdx();
            mFrameConstants->AmbientMapRIdx = mSsaoPass->GetSsaoSrvIdx();
            
            //TAA
            mFrameConstants->VelocityMapIdx = mTaaPass->GetVeloctiySrvIdx();
            mFrameConstants->HistFrameMapIdx = mTaaPass->GetHistFrameSrvIdx();

            gCommandList->Close();
            vector<ID3D12CommandList*> cmdLists{ gCommandList.Get()};
            gCommandQueue->ExecuteCommandLists(1, cmdLists.data());
            FlushCommandQueue();
        }

        void LoadModel(wstring_view path, wstring_view textureDir, wstring_view modelName, XMMATRIX world, bool isSkinned)
        {
            FlushCommandQueue();
            ThrowIfFailed(mInitCommandAllocator->Reset());
            ThrowIfFailed(gCommandList->Reset(mInitCommandAllocator.Get(), nullptr));

            gScene->LoadModel(modelName, path, textureDir, isSkinned);
            gScene->SetWorld(modelName, world);

            ThrowIfFailed(gCommandList->Close());
            vector<ID3D12CommandList*> cmdLists = { gCommandList.Get() };
            gCommandQueue->ExecuteCommandLists(1, cmdLists.data());
            FlushCommandQueue();
            
            gScene->ReleaseIntermediateBuffers(modelName);
        }

        void UnloadModel(wstring_view modelName)
        {
            FlushCommandQueue();
            gScene->UnloadModel(modelName);
        }

        vector<wstring_view> GetAnimationNames(wstring_view modelName)
        {
            return gScene->GetAnimationClips(modelName);
        }

        void SetAnimation(wstring_view modelName, wstring_view animationName)
        {
            gScene->SetAnimationClip(modelName, animationName);
        }

        vector<wstring_view> GetModelNames()
        {
            return gScene->GetModelNames();
        }

    protected:
        void InitConstants()
        {
            mFrameConstants = make_unique<FrameConstants>();
            mFrameCBAllocator = make_unique<FastConstantBufferAllocator>(gNumFrame, sizeof(FrameConstants), gHeapManager->GetUploadBuffersHeap());
        }

        void InitPipelineStates()
        {
            gCullDisabledState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
            gCullDisabledState.CullMode = D3D12_CULL_MODE_NONE;

            gDepthDisabledState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
            gDepthDisabledState.DepthEnable = false;

            gDepthLessEqualState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
            gDepthLessEqualState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

            gAlphaBlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
            gAlphaBlendState.RenderTarget[0].BlendEnable = true;
            gAlphaBlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            gAlphaBlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
            gAlphaBlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            gAlphaBlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            gAlphaBlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
            gAlphaBlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
            gAlphaBlendState.RenderTarget[0].LogicOpEnable = false;
            gAlphaBlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
            gAlphaBlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        void InitFrame()
        {
            gFramePass = make_unique<FramePass>();
        }

        void InitSsao()
        {
            mSsaoPass = make_unique<SsaoPass>();
        }

        void InitNormal()
        {
            mNormalPass = make_unique<NormalPass>();
        }

        void InitTaa()
        {
            mTaaPass = make_unique<TaaPass>();
        }

        void InitMainLight()
        {
            Light light = {};
            light.Direction = { 0.8f,-1.0f,1.0f };
            light.Strength = { 0.8f,0.8f,0.8f };
            mMainLightShadowPass = make_unique<CascadedShadowPass>(light);
        }

        void InitScene()
        {
            gScene = make_unique<Scene>(L"Test");
            
            gScene->LoadGround();
            gScene->LoadSkyBox();
        }

        void UpdateFrameCB()
        {
            XMMATRIX view = mCamera->GetView();
            XMMATRIX invView = XMMatrixInverse(GetRvaluePtr(XMMatrixDeterminant(view)), view);
            XMMATRIX proj = mCamera->GetProj();
            XMMATRIX invProj = XMMatrixInverse(GetRvaluePtr(XMMatrixDeterminant(proj)), proj);
            XMMATRIX viewProj = XMMatrixMultiply(view, proj);
            XMMATRIX invViewProj = XMMatrixInverse(GetRvaluePtr(XMMatrixDeterminant(viewProj)), viewProj);
            
            static XMMATRIX tex(
                0.5f, 0.0f, 0.0f, 0.0f,
                0.0f, -0.5f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.5f, 0.5f, 0.0f, 1.0f);
            XMMATRIX projTex = XMMatrixMultiply(proj, tex);
            XMMATRIX viewProjTex = XMMatrixMultiply(viewProj, tex);
            
            XMStoreFloat4x4(&mFrameConstants->View, XMMatrixTranspose(view));
            XMStoreFloat4x4(&mFrameConstants->InvView, XMMatrixTranspose(invView));
            XMStoreFloat4x4(&mFrameConstants->Proj, XMMatrixTranspose(proj));
            XMStoreFloat4x4(&mFrameConstants->InvProj, XMMatrixTranspose(invProj));
            XMStoreFloat4x4(&mFrameConstants->ViewProj, XMMatrixTranspose(viewProj));
            XMStoreFloat4x4(&mFrameConstants->InvViewProj, XMMatrixTranspose(invViewProj));
            XMStoreFloat4x4(&mFrameConstants->ProjTex, XMMatrixTranspose(projTex));
            XMStoreFloat4x4(&mFrameConstants->ViewProjTex, XMMatrixTranspose(viewProjTex));
            
            XMFLOAT4X4 jitteredProj4x4f = mCamera->GetProj4x4f();
            mTaaPass->GetHalton(jitteredProj4x4f._31, jitteredProj4x4f._32);
            mTaaPass->SetHistViewProj(viewProj);

            XMMATRIX histViewProj = mTaaPass->GetHistViewProj();
            XMMATRIX jitteredProj = XMLoadFloat4x4(&jitteredProj4x4f);
            XMMATRIX jitteredViewProj = XMMatrixMultiply(view, jitteredProj);

            XMStoreFloat4x4(&mFrameConstants->HistViewProj, XMMatrixTranspose(histViewProj));
            XMStoreFloat4x4(&mFrameConstants->JitteredViewProj, XMMatrixTranspose(jitteredViewProj));

            mFrameConstants->EyePosW = mCamera->GetPosition3f();
            mFrameConstants->RenderTargetSize = { static_cast<float>(mClientWidth), static_cast<float>(mClientHeight) };
            mFrameConstants->InvRenderTargetSize = { 1.0f / mClientWidth, 1.0f / mClientHeight };
            mFrameConstants->NearZ = mCamera->GetNearZ();
            mFrameConstants->FarZ = mCamera->GetFarZ();
            
            mSsaoPass->GetOffsetVectors(mFrameConstants->OffsetVectors);
            auto blurWeights = mSsaoPass->CalcGaussWeights(2.5f);
            mFrameConstants->BlurWeights[0] = XMFLOAT4(&blurWeights[0]);
            mFrameConstants->BlurWeights[1] = XMFLOAT4(&blurWeights[4]);
            mFrameConstants->BlurWeights[2] = XMFLOAT4(&blurWeights[8]);

            for (int i = 0; i < mMainLightShadowPass->GetSplitLevel(); ++i)
            {
                mFrameConstants->Lights[i] = mMainLightShadowPass->GetLight(i);
            }

            for (int i = 0; i < mMainLightShadowPass->GetSplitLevel() + 1; ++i)
            {
                mFrameConstants->MainLightSplitZ[i] = mMainLightShadowPass->GetSplitZ(i);
            }

            mFrameConstants->MeshCBIdx = gScene->GetMeshCBIdx();
            mFrameCBAddr = mFrameCBAllocator->Allocate(mFrameConstants.get());
        }

        void ReleaseIntermediateBuffers()
        {
            gScene->ReleaseIntermediateBuffers();
            mSsaoPass->ReleaseIntermediateBuffers();
        }

    protected:
        unique_ptr<SsaoPass> mSsaoPass;
        unique_ptr<NormalPass> mNormalPass;
        unique_ptr<TaaPass> mTaaPass;
        unique_ptr<CascadedShadowPass> mMainLightShadowPass;

        unique_ptr<FrameConstants> mFrameConstants;
        unique_ptr<FastConstantBufferAllocator> mFrameCBAllocator;
        D3D12_GPU_VIRTUAL_ADDRESS mFrameCBAddr;
    };
}