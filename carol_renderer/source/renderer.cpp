#include <renderer.h>
#include <carol.h>
#include <DirectXColors.h>

namespace Carol {
	using std::vector;
	using std::unique_ptr;
	using std::wstring;
	using std::wstring_view;
	using std::to_wstring;
	using std::make_unique;
	using Microsoft::WRL::ComPtr;
	using namespace DirectX;
}

Carol::Renderer::Renderer(HWND hWnd, uint32_t width, uint32_t height)
	:mhWnd(hWnd)
{
#ifdef _DEBUG
	InitDebug();
#endif

	InitDxgiFactory();
	InitDevice();
	InitFence();
	InitCommandQueue();
	InitCommandAllocatorPool();
	InitGraphicsCommandList();
	InitPipelineStates();

	InitHeapManager();
	InitDescriptorManager();
	InitTextureManager();
	InitTimer();
	InitCamera();
	InitScene();
	InitConstants();

	InitRenderPass();
	InitDisplay();
	InitFrame();
	InitMainLight();
	InitNormal();
	InitSsao();
	InitToneMapping();
	InitTaa();

	OnResize(width, height, true);
	ReleaseIntermediateBuffers();

	gGraphicsCommandList->Close();
	vector<ID3D12CommandList*> cmdLists = { gGraphicsCommandList.Get() };
	gCommandQueue->ExecuteCommandLists(1, cmdLists.data());

	++gCpuFenceValue;
	ThrowIfFailed(gCommandQueue->Signal(gFence.Get(), gCpuFenceValue));
}

void Carol::Renderer::InitDebug()
{
	ComPtr<ID3D12Debug5> debugLayer;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(debugLayer.GetAddressOf())));
	debugLayer->EnableDebugLayer();
	debugLayer->SetEnableAutoName(true);

	gDebugLayer = debugLayer;
}

void Carol::Renderer::InitDxgiFactory()
{
	ComPtr<IDXGIFactory4> dxgiFactory;
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory.GetAddressOf())));
	gDxgiFactory = dxgiFactory;
}

void Carol::Renderer::InitDevice()
{
	ComPtr<ID3D12Device2> device;
	ThrowIfFailed(D3D12CreateDevice(device.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(device.GetAddressOf())));
	
	gDevice = device;
}

void Carol::Renderer::InitFence()
{
	ComPtr<ID3D12Fence> fence;
	ThrowIfFailed(gDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())));
	gFence = fence;
	gCpuFenceValue = 0;
	gGpuFenceValue = 0;
}

void Carol::Renderer::InitCommandQueue()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	ThrowIfFailed(gDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(gCommandQueue.GetAddressOf())));
}

void Carol::Renderer::InitCommandAllocatorPool()
{
	gCommandAllocatorPool = make_unique<CommandAllocatorPool>(D3D12_COMMAND_LIST_TYPE_DIRECT);
}

void Carol::Renderer::InitGraphicsCommandList()
{
	gCommandAllocator = gCommandAllocatorPool->RequestAllocator(gGpuFenceValue);
	ComPtr<ID3D12GraphicsCommandList6> cmdList;
	ThrowIfFailed(gDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, gCommandAllocator.Get(), nullptr, IID_PPV_ARGS(cmdList.GetAddressOf())));

	gGraphicsCommandList = cmdList;
}

void Carol::Renderer::InitPipelineStates()
{
	gCullDisabledState = make_unique<D3D12_RASTERIZER_DESC>(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	gCullDisabledState->CullMode = D3D12_CULL_MODE_NONE;

	gDepthDisabledState = make_unique<D3D12_DEPTH_STENCIL_DESC>(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
	gDepthDisabledState->DepthEnable = false;
	gDepthDisabledState->DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

	gDepthLessEqualState = make_unique<D3D12_DEPTH_STENCIL_DESC>(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
	gDepthLessEqualState->DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	gAlphaBlendState = make_unique<D3D12_BLEND_DESC>(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	gAlphaBlendState->RenderTarget[0].BlendEnable = true;
	gAlphaBlendState->RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	gAlphaBlendState->RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	gAlphaBlendState->RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	gAlphaBlendState->RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	gAlphaBlendState->RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
	gAlphaBlendState->RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	gAlphaBlendState->RenderTarget[0].LogicOpEnable = false;
	gAlphaBlendState->RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	gAlphaBlendState->RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
}

void Carol::Renderer::InitHeapManager()
{
	gHeapManager = make_unique<HeapManager>(1 << 29);
	StructuredBuffer::InitCounterResetBuffer(gHeapManager->GetUploadBuffersHeap());
}

void Carol::Renderer::InitDescriptorManager()
{
	gDescriptorManager = make_unique<DescriptorManager>();
}

void Carol::Renderer::InitTextureManager()
{
	gTextureManager = make_unique<TextureManager>();
}

void Carol::Renderer::InitTimer()
{
	mTimer = make_unique<Timer>();
	mTimer->Reset();
}

void Carol::Renderer::InitCamera()
{
	mCamera = make_unique<PerspectiveCamera>();
	mCamera->LookAt(XMFLOAT3(0.f, 10.f, -20.f), XMFLOAT3(0.f, 10.f, 0.f), XMFLOAT3(0.f, 1.f, 0.f));
	mCamera->UpdateViewMatrix();
}

void Carol::Renderer::InitScene()
{
	gScene = make_unique<Scene>(L"Test");
	gScene->LoadSkyBox();
}

void Carol::Renderer::InitConstants()
{
	mFrameConstants = make_unique<FrameConstants>();
	mFrameCBAllocator = make_unique<FastConstantBufferAllocator>(1024, sizeof(FrameConstants), gHeapManager->GetUploadBuffersHeap());
}

void Carol::Renderer::InitRenderPass()
{
	RenderPass::Init();
}

void Carol::Renderer::InitDisplay()
{
	mDisplayPass = make_unique<DisplayPass>(mhWnd, 2);
}

void Carol::Renderer::InitFrame()
{
	mFramePass = make_unique<FramePass>();
}

void Carol::Renderer::InitMainLight()
{
	Light light = {};
	light.Strength = { 1.f,1.f,1.f };
	XMStoreFloat3(&light.Direction, { .8f,-1.f,1.f });

	mMainLightShadowPass = make_unique<CascadedShadowPass>(light);
	mFrameConstants->AmbientColor = { .4f,.4f,.4f };
}

void Carol::Renderer::InitNormal()
{
	mNormalPass = make_unique<NormalPass>();
}

void Carol::Renderer::InitSsao()
{
	mSsaoPass = make_unique<SsaoPass>();
}

void Carol::Renderer::InitTaa()
{
	mTaaPass = make_unique<TaaPass>();
}

void Carol::Renderer::InitToneMapping()
{
	mToneMappingPass = make_unique<ToneMappingPass>();
}

float Carol::Renderer::AspectRatio()
{
	return 1.0f * mClientWidth / mClientHeight;
}

void Carol::Renderer::FlushCommandQueue()
{
	++gCpuFenceValue;
	ThrowIfFailed(gCommandQueue->Signal(gFence.Get(), gCpuFenceValue));

	if (gFence->GetCompletedValue() < gCpuFenceValue)
	{
		auto eventHandle = CreateEventEx(nullptr, LPCWSTR(nullptr), 0, EVENT_ALL_ACCESS);
		ThrowIfFailed(gFence->SetEventOnCompletion(gCpuFenceValue, eventHandle));

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

void Carol::Renderer::ReleaseIntermediateBuffers()
{
	gScene->ReleaseIntermediateBuffers();
	mSsaoPass->ReleaseIntermediateBuffers();
}

void Carol::Renderer::Draw()
{	
	gCommandAllocatorPool->DiscardAllocator(gCommandAllocator.Get(), gCpuFenceValue);
	gCommandAllocator = gCommandAllocatorPool->RequestAllocator(gGpuFenceValue);
	ThrowIfFailed(gGraphicsCommandList->Reset(gCommandAllocator.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = {gDescriptorManager->GetResourceDescriptorHeap()};
	gGraphicsCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	gGraphicsCommandList->SetGraphicsRootSignature(RenderPass::GetRootSignature()->Get());
	gGraphicsCommandList->SetComputeRootSignature(RenderPass::GetRootSignature()->Get());

	gGraphicsCommandList->SetGraphicsRootConstantBufferView(FRAME_CB, mFrameCBAddr);
	gGraphicsCommandList->SetComputeRootConstantBufferView(FRAME_CB, mFrameCBAddr);

	mMainLightShadowPass->Draw();
	mFramePass->Cull();
	
	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		MeshType type = (MeshType)i;
		const StructuredBuffer* indirectCommandBuffer = mFramePass->GetIndirectCommandBuffer(type);
		mNormalPass->SetIndirectCommandBuffer(type, indirectCommandBuffer);
		mTaaPass->SetIndirectCommandBuffer(type, indirectCommandBuffer);
	}

	mNormalPass->Draw();
	mSsaoPass->Draw();
	mFramePass->Draw();

	// Post process
	mToneMappingPass->Draw();
	mTaaPass->Draw();
	mDisplayPass->Draw();
	
	ThrowIfFailed(gGraphicsCommandList->Close());
	vector<ID3D12CommandList*> cmdLists{ gGraphicsCommandList.Get()};
	gCommandQueue->ExecuteCommandLists(1, cmdLists.data());

	mDisplayPass->Present();
	++gCpuFenceValue;
	ThrowIfFailed(gCommandQueue->Signal(gFence.Get(), gCpuFenceValue));
}

void Carol::Renderer::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhWnd);
}

void Carol::Renderer::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void Carol::Renderer::OnMouseMove(WPARAM btnState, int x, int y)
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

void Carol::Renderer::OnKeyboardInput()
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

void Carol::Renderer::CalcFrameState()
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

		SetWindowText(mhWnd, windowText.c_str());

		// Reset for next average.
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

void Carol::Renderer::Tick()
{
	mTimer->Tick();
}

void Carol::Renderer::Stop()
{
	mTimer->Stop();
}

void Carol::Renderer::Start()
{
	mTimer->Start();
}

void Carol::Renderer::SetPaused(bool state)
{
	mPaused = state;
}

bool Carol::Renderer::IsPaused()
{
	return mPaused;
}

void Carol::Renderer::SetMaximized(bool state)
{
	mMaximized = state;
}

bool Carol::Renderer::IsZoomed()
{
	return mMaximized;
}

void Carol::Renderer::SetMinimized(bool state)
{
	mMinimized = state;
}

bool Carol::Renderer::IsIconic()
{
	return mMinimized;
}

void Carol::Renderer::SetResizing(bool state)
{
	mResizing = state;
}

bool Carol::Renderer::IsResizing()
{
	return mResizing;
}

void Carol::Renderer::Update()
{
	OnKeyboardInput();
	gGpuFenceValue = gFence->GetCompletedValue();

	gDescriptorManager->DelayedDelete(gCpuFenceValue, gGpuFenceValue);
	gHeapManager->DelayedDelete(gCpuFenceValue, gGpuFenceValue);

	gScene->Update(mTimer.get(), gCpuFenceValue, gGpuFenceValue);
	mMainLightShadowPass->Update(dynamic_cast<PerspectiveCamera*>(mCamera.get()), 0.85f);
	mCamera->UpdateViewMatrix();

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
	mFramePass->Update(XMLoadFloat4x4(&mFrameConstants->ViewProj), XMLoadFloat4x4(&mFrameConstants->HistViewProj), XMLoadFloat3(&mFrameConstants->EyePosW));

	mFrameConstants->EyePosW = mCamera->GetPosition3f();
	mFrameConstants->RenderTargetSize = { static_cast<float>(mClientWidth), static_cast<float>(mClientHeight) };
	mFrameConstants->InvRenderTargetSize = { 1.0f / mClientWidth, 1.0f / mClientHeight };
	mFrameConstants->NearZ = dynamic_cast<PerspectiveCamera*>(mCamera.get())->GetNearZ();
	mFrameConstants->FarZ = dynamic_cast<PerspectiveCamera*>(mCamera.get())->GetFarZ();
	
	mSsaoPass->GetOffsetVectors(mFrameConstants->OffsetVectors);
	auto blurWeights = mSsaoPass->CalcGaussWeights(2.5f);
	mFrameConstants->GaussBlurWeights[0] = XMFLOAT4(&blurWeights[0]);
    mFrameConstants->GaussBlurWeights[1] = XMFLOAT4(&blurWeights[4]);
    mFrameConstants->GaussBlurWeights[2] = XMFLOAT4(&blurWeights[8]);

	for (int i = 0; i < mMainLightShadowPass->GetSplitLevel(); ++i)
	{
		mFrameConstants->MainLights[i] = mMainLightShadowPass->GetLight(i);
	}

	for (int i = 0; i < mMainLightShadowPass->GetSplitLevel() + 1; ++i)
	{
		mFrameConstants->MainLightSplitZ[i] = mMainLightShadowPass->GetSplitZ(i);
	}

	mFramePass->SetFrameMap(mDisplayPass->GetFrameMap());
	mToneMappingPass->SetFrameMap(mDisplayPass->GetFrameMap());
	mTaaPass->SetFrameMap(mDisplayPass->GetFrameMap());

	mFrameConstants->MeshBufferIdx = gScene->GetMeshBufferIdx();
	mFrameConstants->CommandBufferIdx = gScene->GetCommandBufferIdx();
	mFrameConstants->RWFrameMapIdx = mDisplayPass->GetFrameMapUavIdx();

	mFrameCBAddr = mFrameCBAllocator->Allocate(mFrameConstants.get());
}

void Carol::Renderer::OnResize(uint32_t width, uint32_t height, bool init)
{
	if (mClientWidth == width && mClientHeight == height)
	{
		return;
	}

	mClientWidth = width;
	mClientHeight = height;

	mTimer->Start();
	dynamic_cast<PerspectiveCamera*>(mCamera.get())->SetLens(0.25f * DirectX::XM_PI, AspectRatio(), 1.0f, 1000.0f);

	mDisplayPass->OnResize(width, height);
	mFramePass->OnResize(width, height);
	mNormalPass->OnResize(width, height);
	mSsaoPass->OnResize(width, height);

	mToneMappingPass->OnResize(width, height);
	mTaaPass->OnResize(width, height);

	mFramePass->SetDepthStencilMap(mDisplayPass->GetDepthStencilMap());
	mNormalPass->SetDepthStencilMap(mDisplayPass->GetDepthStencilMap());
	mTaaPass->SetDepthStencilMap(mDisplayPass->GetDepthStencilMap());

	mFrameConstants->InstanceFrustumCulledMarkBufferIdx = gScene->GetInstanceFrustumCulledMarkBufferIdx();
	mFrameConstants->InstanceOcclusionCulledMarkBufferIdx = gScene->GetInstanceOcclusionCulledMarkBufferIdx();
	mFrameConstants->InstanceCulledMarkBufferIdx = gScene->GetInstanceCulledMarkBufferIdx();
	
	mFrameConstants->DepthStencilMapIdx = mDisplayPass->GetDepthStencilSrvIdx();
	mFrameConstants->NormalMapIdx = mNormalPass->GetNormalSrvIdx();

	// Main light
	for (int i = 0; i < mMainLightShadowPass->GetSplitLevel(); ++i)
	{
		mFrameConstants->MainLightShadowMapIdx[i] = mMainLightShadowPass->GetShadowSrvIdx(i);
	}

	// OITPPLL
	mFrameConstants->RWOitBufferIdx = mFramePass->GetPpllUavIdx();
	mFrameConstants->RWOitOffsetBufferIdx = mFramePass->GetOffsetUavIdx();
	mFrameConstants->OitCounterIdx = mFramePass->GetCounterUavIdx();
	mFrameConstants->OitBufferIdx = mFramePass->GetPpllSrvIdx();
	mFrameConstants->OitOffsetBufferIdx = mFramePass->GetOffsetSrvIdx();
	
	// SSAO
	mFrameConstants->RandVecMapIdx = mSsaoPass->GetRandVecSrvIdx();
	mFrameConstants->RWAmbientMapIdx = mSsaoPass->GetSsaoUavIdx();
	mFrameConstants->AmbientMapIdx = mSsaoPass->GetSsaoSrvIdx();
	
	// TAA
	mFrameConstants->VelocityMapIdx = mTaaPass->GetVeloctiySrvIdx();
	mFrameConstants->RWHistMapIdx = mTaaPass->GetHistFrameUavIdx();
}

void Carol::Renderer::LoadModel(wstring_view path, wstring_view textureDir, wstring_view modelName, DirectX::XMMATRIX world, bool isSkinned)
{
	gCommandAllocatorPool->DiscardAllocator(gCommandAllocator.Get(), gCpuFenceValue);
	gCommandAllocator = gCommandAllocatorPool->RequestAllocator(gGpuFenceValue);
	ThrowIfFailed(gGraphicsCommandList->Reset(gCommandAllocator.Get(), nullptr));

	gScene->LoadModel(
		modelName,
		path,
		textureDir,
		isSkinned);
	gScene->SetWorld(modelName, world);
	gScene->ReleaseIntermediateBuffers(modelName);

	gGraphicsCommandList->Close();
	vector<ID3D12CommandList*> cmdLists = { gGraphicsCommandList.Get() };
	gCommandQueue->ExecuteCommandLists(1, cmdLists.data());

	++gCpuFenceValue;
	ThrowIfFailed(gCommandQueue->Signal(gFence.Get(), gCpuFenceValue));
}

void Carol::Renderer::UnloadModel(wstring_view modelName)
{
	gScene->UnloadModel(modelName);
}

Carol::vector<Carol::wstring_view> Carol::Renderer::GetAnimationNames(wstring_view modelName)
{
	return gScene->GetAnimationClips(modelName);
}

void Carol::Renderer::SetAnimation(wstring_view modelName, wstring_view animationName)
{
	gScene->SetAnimationClip(modelName, animationName);
}

Carol::vector<Carol::wstring_view> Carol::Renderer::GetModelNames()
{
	return gScene->GetModelNames();
}
