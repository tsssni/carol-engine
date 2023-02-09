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
        uint32_t MainLightShadowMapIdx[MAIN_LIGHT_SPLIT_LEVEL] = {0};
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
            : mhWnd(hWnd)
        {
            InitTimer();
            InitCamera();

#if defined _DEBUG
            InitDebug();
#endif
            InitDxgiFactory();
            InitDevice();
            InitFence();

            InitCommandQueue();
            InitCommandAllocatorPool();
            InitCommandList();

            InitHeapManager();
            InitDescriptorManager();
            InitTextureManager();

            InitRenderPass();
            InitDisplay();
        }

        BaseRenderer(const BaseRenderer &) = delete;
        BaseRenderer(BaseRenderer &&) = delete;
        BaseRenderer &operator=(const BaseRenderer &) = delete;

        ~BaseRenderer()
        {
            FlushCommandQueue();
            StructuredBuffer::DeleteCounterResetBuffer();
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
            dynamic_cast<PerspectiveCamera *>(mCamera.get())->SetLens(0.25f * DirectX::XM_PI, AspectRatio(), 1.0f, 1000.0f);
            mDisplayPass->OnResize(mClientWidth, mClientHeight, mDevice.Get(), mHeapManager->GetDefaultBuffersHeap(), mDescriptorManager.get());
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

            mDevice = device;
        }

        virtual void InitFence()
        {
            ComPtr<ID3D12Fence> fence;
            ThrowIfFailed(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())));
            mFence = fence;
        }

        virtual void InitCommandQueue()
        {
            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

            ThrowIfFailed(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(mCommandQueue.GetAddressOf())));
        }

        virtual void InitCommandAllocatorPool()
        {
            mCommandAllocatorPool = make_unique<CommandAllocatorPool>(D3D12_COMMAND_LIST_TYPE_DIRECT, mDevice.Get());
        }

        virtual void InitCommandList()
        {
            mCommandAllocator = mCommandAllocatorPool->RequestAllocator(mGpuFenceValue);
            ComPtr<ID3D12GraphicsCommandList6> cmdList;
            ThrowIfFailed(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocator.Get(), nullptr, IID_PPV_ARGS(cmdList.GetAddressOf())));

            mCommandList = cmdList;
        }

        virtual void InitHeapManager()
        {
            mHeapManager = make_unique<HeapManager>(mDevice.Get(), 1 << 27);
            StructuredBuffer::InitCounterResetBuffer(mHeapManager->GetUploadBuffersHeap(), mDevice.Get());
        }

        virtual void InitDescriptorManager()
        {
            mDescriptorManager = make_unique<DescriptorManager>(mDevice.Get());
        }

        virtual void InitTextureManager()
        {
            mTextureManager = make_unique<TextureManager>();
        }

        virtual void InitRenderPass()
        {
            RenderPass::Init(mDevice.Get());
        }

        virtual void InitDisplay()
        {
            mDisplayPass = make_unique<DisplayPass>(mhWnd, mDxgiFactory.Get(), mCommandQueue.Get(), 2);
        }

        virtual void FlushCommandQueue()
        {
            ++mCpuFenceValue;
            ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCpuFenceValue));

            if (mFence->GetCompletedValue() < mCpuFenceValue)
            {
                auto eventHandle = CreateEventExW(nullptr, LPCWSTR(nullptr), 0, EVENT_ALL_ACCESS);
                ThrowIfFailed(mFence->SetEventOnCompletion(mCpuFenceValue, eventHandle));

                WaitForSingleObject(eventHandle, INFINITE);
                CloseHandle(eventHandle);
            }
        }

    protected:
        ComPtr<ID3D12Debug> mDebugLayer;
        ComPtr<IDXGIFactory> mDxgiFactory;
        ComPtr<ID3D12Device> mDevice;

        ComPtr<ID3D12CommandQueue> mCommandQueue;
		ComPtr<ID3D12GraphicsCommandList> mCommandList;
        ComPtr<ID3D12CommandAllocator> mCommandAllocator;
		unique_ptr<CommandAllocatorPool> mCommandAllocatorPool;

		ComPtr<ID3D12Fence> mFence;
		uint64_t mCpuFenceValue = 0;
		uint64_t mGpuFenceValue = 0;

        unique_ptr<DescriptorManager> mDescriptorManager;
        unique_ptr<HeapManager> mHeapManager;
        unique_ptr<TextureManager> mTextureManager;

        unique_ptr<DisplayPass> mDisplayPass;

        uint32_t mClientWidth = 0;
        uint32_t mClientHeight = 0;
        unique_ptr<Camera> mCamera;
        unique_ptr<Timer> mTimer;

        HWND mhWnd;
        wstring mMainWndCaption = L"Carol";
        D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;

        DirectX::XMINT2 mLastMousePos = {0, 0};

        bool mPaused = false;
        bool mMaximized = false;
        bool mMinimized = false;
        bool mResizing = false;
    };

    export class Renderer : public BaseRenderer
    {
    public:
        Renderer(HWND hWnd, uint32_t width, uint32_t height)
            : BaseRenderer(hWnd)
        {
            InitConstants();
            InitPipelineStates();
            InitScene();
            InitFrame();
            InitNormal();
            InitMainLight();
            InitSsao();
            InitTaa();
            OnResize(width, height, true);
            ReleaseIntermediateBuffers();

            mCommandList->Close();
            vector<ID3D12CommandList*> cmdLists = { mCommandList.Get() };
            mCommandQueue->ExecuteCommandLists(1, cmdLists.data());

            ++mCpuFenceValue;
            ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCpuFenceValue));
        }

        virtual void Draw() override
        {
            mCommandAllocatorPool->DiscardAllocator(mCommandAllocator.Get(), mCpuFenceValue);
            mCommandAllocator = mCommandAllocatorPool->RequestAllocator(mGpuFenceValue);
            ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

            ID3D12DescriptorHeap* descriptorHeaps[] = {mDescriptorManager->GetResourceDescriptorHeap()};
            mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

            mTaaPass->SetCurrBackBuffer(mDisplayPass->GetCurrBackBuffer());
            mTaaPass->SetCurrBackBufferRtv(mDisplayPass->GetCurrBackBufferRtv());

            mCommandList->SetGraphicsRootSignature(RenderPass::GetRootSignature()->Get());
            mCommandList->SetComputeRootSignature(RenderPass::GetRootSignature()->Get());

            mCommandList->SetGraphicsRootConstantBufferView(FRAME_CB, mFrameCBAddr);
            mCommandList->SetComputeRootConstantBufferView(FRAME_CB, mFrameCBAddr);

            mMainLightShadowPass->Draw(mCommandList.Get());
            mFramePass->Cull(mCommandList.Get());
            
            for (int i = 0; i < MESH_TYPE_COUNT; ++i)
            {
                MeshType type = (MeshType)i;
                const StructuredBuffer* indirectCommandBuffer = mFramePass->GetIndirectCommandBuffer(type);
                mNormalPass->SetIndirectCommandBuffer(type, indirectCommandBuffer);
                mTaaPass->SetIndirectCommandBuffer(type, indirectCommandBuffer);
            }

            mNormalPass->Draw(mCommandList.Get());
            mSsaoPass->Draw(mCommandList.Get());
            mFramePass->Draw(mCommandList.Get());
            mTaaPass->Draw(mCommandList.Get());
            
            ThrowIfFailed(mCommandList->Close());
            vector<ID3D12CommandList*> cmdLists{ mCommandList.Get()};
            mCommandQueue->ExecuteCommandLists(1, cmdLists.data());

            mDisplayPass->Present();
            ++mCpuFenceValue;
            ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCpuFenceValue));
        }

        virtual void Update() override
        {
            OnKeyboardInput();
            mGpuFenceValue = mFence->GetCompletedValue();

            mDescriptorManager->DelayedDelete(mCpuFenceValue, mGpuFenceValue);
            mHeapManager->DelayedDelete(mCpuFenceValue, mGpuFenceValue);

            mCamera->UpdateViewMatrix();
            mScene->Update(mTimer.get(), mCpuFenceValue, mGpuFenceValue);
            mMainLightShadowPass->Update(dynamic_cast<PerspectiveCamera*>(mCamera.get()), mCpuFenceValue, mGpuFenceValue, 0.85f);
            mFramePass->Update(mCpuFenceValue, mGpuFenceValue);

            UpdateFrameCB();
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
            ID3D12Device *device = mDevice.Get();
            Heap *heap = mHeapManager->GetDefaultBuffersHeap();
            DescriptorManager *descriptorManager = mDescriptorManager.get();

            BaseRenderer::OnResize(width, height, init);
            mFramePass->OnResize(width, height, device, heap, descriptorManager);
            mNormalPass->OnResize(width, height, device, heap, descriptorManager);
            mSsaoPass->OnResize(width, height, device, heap, descriptorManager);
            mTaaPass->OnResize(width, height, device, heap, descriptorManager);

            mNormalPass->SetFrameDsv(mFramePass->GetFrameDsv());
            mTaaPass->SetFrameDsv(mFramePass->GetFrameDsv());

            mFrameConstants->InstanceFrustumCulledMarkIdx = mScene->GetInstanceFrustumCulledMarkBufferIdx();
            mFrameConstants->InstanceOcclusionPassedMarkIdx = mScene->GetInstanceOcclusionPassedMarkBufferIdx();

            mFrameConstants->FrameMapIdx = mFramePass->GetFrameSrvIdx();
            mFrameConstants->DepthStencilMapIdx = mFramePass->GetDepthStencilSrvIdx();
            mFrameConstants->NormalMapIdx = mNormalPass->GetNormalSrvIdx();

            // Main light
            for (int i = 0; i < mMainLightShadowPass->GetSplitLevel(); ++i)
            {
                mFrameConstants->MainLightShadowMapIdx[i] = mMainLightShadowPass->GetShadowSrvIdx(i);
            }

            // OITPPLL
            mFrameConstants->OitBufferWIdx = mFramePass->GetPpllUavIdx();
            mFrameConstants->OitOffsetBufferWIdx = mFramePass->GetOffsetUavIdx();
            mFrameConstants->OitCounterIdx = mFramePass->GetCounterUavIdx();
            mFrameConstants->OitBufferRIdx = mFramePass->GetPpllSrvIdx();
            mFrameConstants->OitOffsetBufferRIdx = mFramePass->GetOffsetSrvIdx();

            // SSAO
            mFrameConstants->RandVecMapIdx = mSsaoPass->GetRandVecSrvIdx();
            mFrameConstants->AmbientMapWIdx = mSsaoPass->GetSsaoUavIdx();
            mFrameConstants->AmbientMapRIdx = mSsaoPass->GetSsaoSrvIdx();

            // TAA
            mFrameConstants->VelocityMapIdx = mTaaPass->GetVeloctiySrvIdx();
            mFrameConstants->HistFrameMapIdx = mTaaPass->GetHistFrameSrvIdx();
        }

        void LoadModel(wstring_view path, wstring_view textureDir, wstring_view modelName, XMMATRIX world, bool isSkinned)
        {
            mCommandAllocatorPool->DiscardAllocator(mCommandAllocator.Get(), mCpuFenceValue);
            mCommandAllocator = mCommandAllocatorPool->RequestAllocator(mGpuFenceValue);
            ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

            mScene->LoadModel(
                modelName,
                path,
                textureDir,
                isSkinned,
                mDevice.Get(),
                mCommandList.Get(),
                mHeapManager->GetDefaultBuffersHeap(),
                mHeapManager->GetUploadBuffersHeap(),
                mDescriptorManager.get(),
                mTextureManager.get());
            mScene->SetWorld(modelName, world);
            mScene->ReleaseIntermediateBuffers(modelName);

            mCommandList->Close();
            vector<ID3D12CommandList*> cmdLists = { mCommandList.Get() };
            mCommandQueue->ExecuteCommandLists(1, cmdLists.data());

            ++mCpuFenceValue;
            ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCpuFenceValue));
        }

        void UnloadModel(wstring_view modelName)
        {
            mScene->UnloadModel(modelName);
        }

        vector<wstring_view> GetAnimationNames(wstring_view modelName)
        {
            return mScene->GetAnimationClips(modelName);
        }

        void SetAnimation(wstring_view modelName, wstring_view animationName)
        {
            mScene->SetAnimationClip(modelName, animationName);
        }

        vector<wstring_view> GetModelNames()
        {
            return mScene->GetModelNames();
        }

    protected:
        void InitConstants()
        {
            mFrameConstants = make_unique<FrameConstants>();
            mFrameCBAllocator = make_unique<FastConstantBufferAllocator>(1024, sizeof(FrameConstants), mDevice.Get(), mHeapManager->GetUploadBuffersHeap(), mDescriptorManager.get());
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

        void InitScene()
        {
            mScene = make_unique<Scene>(
                L"Test",
                mDevice.Get(),
                mHeapManager->GetDefaultBuffersHeap(),
                mHeapManager->GetUploadBuffersHeap(),
                mDescriptorManager.get());

            mScene->LoadGround(
                mDevice.Get(),
                mCommandList.Get(),
                mHeapManager->GetDefaultBuffersHeap(),
                mHeapManager->GetUploadBuffersHeap(),
                mDescriptorManager.get(),
                mTextureManager.get());

            mScene->LoadSkyBox(
                mDevice.Get(),
                mCommandList.Get(),
                mHeapManager->GetDefaultBuffersHeap(),
                mHeapManager->GetUploadBuffersHeap(),
                mDescriptorManager.get(),
                mTextureManager.get());
        }

        void InitFrame()
        {
            mFramePass = make_unique<FramePass>(
                mDevice.Get(),
                mHeapManager->GetDefaultBuffersHeap(),
                mDescriptorManager.get(),
                mScene.get());
        }

        void InitSsao()
        {
            mSsaoPass = make_unique<SsaoPass>(
                mDevice.Get(),
                mCommandList.Get(),
                mHeapManager->GetDefaultBuffersHeap(),
                mHeapManager->GetUploadBuffersHeap(),
                mDescriptorManager.get());
        }

        void InitNormal()
        {
            mNormalPass = make_unique<NormalPass>(
                mDevice.Get(),
                mFramePass->GetFrameDsvFormat());
        }

        void InitTaa()
        {
            mTaaPass = make_unique<TaaPass>(
                mDevice.Get(),
                mFramePass->GetFrameFormat(),
                mFramePass->GetFrameDsvFormat());
        }

        void InitMainLight()
        {
            Light light = {};
            light.Direction = {0.8f, -1.0f, 1.0f};
            light.Strength = {0.8f, 0.8f, 0.8f};
            mMainLightShadowPass = make_unique<CascadedShadowPass>(
                mDevice.Get(),
                mHeapManager->GetDefaultBuffersHeap(),
                mDescriptorManager.get(),
                mScene.get(),
                light);
        }

        void UpdateFrameCB()
        {
            XMMATRIX view = mCamera->GetView();
            XMMATRIX invView = XMMatrixInverse(GetRvaluePtr(XMMatrixDeterminant(view)), view);
            XMMATRIX proj = mCamera->GetProj();
            XMMATRIX invProj = XMMatrixInverse(GetRvaluePtr(XMMatrixDeterminant(proj)), proj);
            XMMATRIX viewProj = XMMatrixMultiply(view, proj);
            XMMATRIX invViewProj = XMMatrixInverse(GetRvaluePtr(XMMatrixDeterminant(viewProj)), viewProj);

            XMStoreFloat4x4(&mFrameConstants->View, XMMatrixTranspose(view));
            XMStoreFloat4x4(&mFrameConstants->InvView, XMMatrixTranspose(invView));
            XMStoreFloat4x4(&mFrameConstants->Proj, XMMatrixTranspose(proj));
            XMStoreFloat4x4(&mFrameConstants->InvProj, XMMatrixTranspose(invProj));
            XMStoreFloat4x4(&mFrameConstants->ViewProj, XMMatrixTranspose(viewProj));
            XMStoreFloat4x4(&mFrameConstants->InvViewProj, XMMatrixTranspose(invViewProj));

            XMFLOAT4X4 jitteredProj4x4f = mCamera->GetProj4x4f();
            mTaaPass->GetHalton(jitteredProj4x4f._31, jitteredProj4x4f._32);
            mTaaPass->SetHistViewProj(viewProj);

            XMMATRIX histViewProj = mTaaPass->GetHistViewProj();
            XMMATRIX jitteredProj = XMLoadFloat4x4(&jitteredProj4x4f);
            XMMATRIX jitteredViewProj = XMMatrixMultiply(view, jitteredProj);

            XMStoreFloat4x4(&mFrameConstants->HistViewProj, XMMatrixTranspose(histViewProj));
            XMStoreFloat4x4(&mFrameConstants->JitteredViewProj, XMMatrixTranspose(jitteredViewProj));

            mFrameConstants->EyePosW = mCamera->GetPosition3f();
            mFrameConstants->RenderTargetSize = {static_cast<float>(mClientWidth), static_cast<float>(mClientHeight)};
            mFrameConstants->InvRenderTargetSize = {1.0f / mClientWidth, 1.0f / mClientHeight};
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

            mFrameConstants->MeshCBIdx = mScene->GetMeshCBIdx();
            mFrameConstants->CommandBufferIdx = mScene->GetCommandBufferIdx();
            mFrameCBAddr = mFrameCBAllocator->Allocate(mFrameConstants.get());
        }

        void ReleaseIntermediateBuffers()
        {
            mScene->ReleaseIntermediateBuffers();
            mSsaoPass->ReleaseIntermediateBuffers();
        }

    protected:
        unique_ptr<Scene> mScene;

        unique_ptr<FramePass> mFramePass;
        unique_ptr<SsaoPass> mSsaoPass;
        unique_ptr<NormalPass> mNormalPass;
        unique_ptr<TaaPass> mTaaPass;
        unique_ptr<CascadedShadowPass> mMainLightShadowPass;

        unique_ptr<FrameConstants> mFrameConstants;
        unique_ptr<FastConstantBufferAllocator> mFrameCBAllocator;
        D3D12_GPU_VIRTUAL_ADDRESS mFrameCBAddr;
    };
}