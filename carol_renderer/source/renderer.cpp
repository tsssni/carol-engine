#include <renderer.h>
#include <carol.h>
#include <DirectXColors.h>

namespace
{
	using DirectX::operator*;
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
	InitRootSignature();
	InitCommandSignature();

	InitHeapManager();
	InitDescriptorManager();
	InitShaderManager();
	InitTextureManager();
	InitModelManager();
	InitTimer();
	InitCamera();

	InitConstants();
	InitSkyBox();
	InitRandomVectors();
	InitGaussWeights();

	InitCullPass();
	InitDisplayPass();
	InitGeometryPass();
	InitOitppllPass();
	InitShadePass();
	InitMainLightShadowPass();
	InitSsaoPass();
	InitSsgiPass();
	InitToneMappingPass();
	InitTaaPass();
	InitUtilsPass();

	OnResize(width, height, true);

	gGraphicsCommandList->Close();
	std::vector<ID3D12CommandList*> cmdLists = { gGraphicsCommandList.Get() };
	gCommandQueue->ExecuteCommandLists(1, cmdLists.data());

	++gCpuFenceValue;
	ThrowIfFailed(gCommandQueue->Signal(gFence.Get(), gCpuFenceValue));
}

void Carol::Renderer::InitDebug()
{
	Microsoft::WRL::ComPtr<ID3D12Debug5> debugLayer;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(debugLayer.GetAddressOf())));
	debugLayer->EnableDebugLayer();
	debugLayer->SetEnableAutoName(true);

	gDebugLayer = debugLayer;
}

void Carol::Renderer::InitDxgiFactory()
{
	Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory;
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory.GetAddressOf())));
	gDxgiFactory = dxgiFactory;
}

void Carol::Renderer::InitDevice()
{
	Microsoft::WRL::ComPtr<ID3D12Device2> device;
	ThrowIfFailed(D3D12CreateDevice(device.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(device.GetAddressOf())));
	
	gDevice = device;
}

void Carol::Renderer::InitFence()
{
	Microsoft::WRL::ComPtr<ID3D12Fence> fence;
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
	gCommandAllocatorPool = std::make_unique<CommandAllocatorPool>(D3D12_COMMAND_LIST_TYPE_DIRECT);
}

void Carol::Renderer::InitGraphicsCommandList()
{
	gCommandAllocator = gCommandAllocatorPool->RequestAllocator(gGpuFenceValue);
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList6> cmdList;
	ThrowIfFailed(gDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, gCommandAllocator.Get(), nullptr, IID_PPV_ARGS(cmdList.GetAddressOf())));

	gGraphicsCommandList = cmdList;
}

void Carol::Renderer::InitRootSignature()
{
	gRootSignature = std::make_unique<RootSignature>();
}

void Carol::Renderer::InitCommandSignature()
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

void Carol::Renderer::InitHeapManager()
{
	gHeapManager = std::make_unique<HeapManager>(1 << 29);
	StructuredBuffer::InitCounterResetBuffer(gHeapManager->GetUploadBuffersHeap());
}

void Carol::Renderer::InitDescriptorManager()
{
	gDescriptorManager = std::make_unique<DescriptorManager>();
}

void Carol::Renderer::InitShaderManager()
{
	gShaderManager = std::make_unique<ShaderManager>();
}

void Carol::Renderer::InitTextureManager()
{
	gTextureManager = std::make_unique<TextureManager>();
}

void Carol::Renderer::InitTimer()
{
	mTimer = std::make_unique<Timer>();
	mTimer->Reset();
}

void Carol::Renderer::InitCamera()
{
	mCamera = std::make_unique<PerspectiveCamera>();
	mCamera->LookAt(DirectX::XMFLOAT3(0.f, 10.f, -20.f), DirectX::XMFLOAT3(0.f, 10.f, 0.f), DirectX::XMFLOAT3(0.f, 1.f, 0.f));
	mCamera->UpdateViewMatrix();
}

void Carol::Renderer::InitModelManager()
{
	gModelManager = std::make_unique<ModelManager>("Carol");
}

void Carol::Renderer::InitConstants()
{
	mFrameConstants = std::make_unique<FrameConstants>();
	mFrameCBAllocator = std::make_unique<FastConstantBufferAllocator>(1024, sizeof(FrameConstants), gHeapManager->GetUploadBuffersHeap());
}

void Carol::Renderer::InitSkyBox()
{
	mFrameConstants->SkyBoxIdx = gTextureManager->LoadTexture("texture/snowcube1024.dds", false);
}

void Carol::Renderer::InitRandomVectors()
{
	DirectX::XMFLOAT4 offsets[14];

	offsets[0] = DirectX::XMFLOAT4(+1.0f, +1.0f, +1.0f, 0.0f);
	offsets[1] = DirectX::XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);

	offsets[2] = DirectX::XMFLOAT4(-1.0f, +1.0f, +1.0f, 0.0f);
	offsets[3] = DirectX::XMFLOAT4(+1.0f, -1.0f, -1.0f, 0.0f);

	offsets[4] = DirectX::XMFLOAT4(+1.0f, +1.0f, -1.0f, 0.0f);
	offsets[5] = DirectX::XMFLOAT4(-1.0f, -1.0f, +1.0f, 0.0f);

	offsets[6] = DirectX::XMFLOAT4(-1.0f, +1.0f, -1.0f, 0.0f);
	offsets[7] = DirectX::XMFLOAT4(+1.0f, -1.0f, +1.0f, 0.0f);

	// 6 centers of cube faces
	offsets[8] = DirectX::XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
	offsets[9] = DirectX::XMFLOAT4(+1.0f, 0.0f, 0.0f, 0.0f);

	offsets[10] = DirectX::XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
	offsets[11] = DirectX::XMFLOAT4(0.0f, +1.0f, 0.0f, 0.0f);

	offsets[12] = DirectX::XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
	offsets[13] = DirectX::XMFLOAT4(0.0f, 0.0f, +1.0f, 0.0f);

	for (int i = 0; i < 14; ++i)
	{
		float s = 0.25f + rand() * 1.0f / RAND_MAX * (1.0f - 0.25f);
		DirectX::XMVECTOR v = s * DirectX::XMVector4Normalize(DirectX::XMLoadFloat4(&offsets[i]));

		DirectX::XMStoreFloat4(&mFrameConstants->OffsetVectors[i], v);
	}
}

void Carol::Renderer::InitGaussWeights()
{
	constexpr int maxGaussRadius = 5;
	constexpr float sigma = 2.5;

	int blurRadius = (int)ceil(2.0f * sigma);
	assert(blurRadius <= maxGaussRadius);

	std::vector<float> weights;
	float weightsSum = 0.0f;

	weights.resize(2 * blurRadius + 1);

	for (int i = -blurRadius; i <= blurRadius; ++i)
	{
		weights[blurRadius + i] = expf(-i * i / (2.0f * sigma * sigma));
		weightsSum += weights[blurRadius + i];
	}

	for (int i = 0; i < blurRadius * 2 + 1; ++i)
	{
		mFrameConstants->GaussBlurWeights[i] = weights[i] / weightsSum;
	}
}

void Carol::Renderer::InitCullPass()
{
	mCullPass = std::make_unique<CullPass>();
}

void Carol::Renderer::InitDisplayPass()
{
	mDisplayPass = std::make_unique<DisplayPass>(mhWnd, 2);
}

void Carol::Renderer::InitGeometryPass()
{
	mGeometryPass = std::make_unique<GeometryPass>();
}

void Carol::Renderer::InitOitppllPass()
{
	mOitppllPass = std::make_unique<OitppllPass>();
}

void Carol::Renderer::InitShadePass()
{
	mShadePass = std::make_unique<ShadePass>();
}

void Carol::Renderer::InitMainLightShadowPass()
{
	Light light = {};
	light.Strength = { 2.f,1.9f,1.6f };
	DirectX::XMStoreFloat3(&light.Direction, { .4f,-1.f,.2f });

	mMainLightShadowPass = std::make_unique<CascadedShadowPass>(light);
	mFrameConstants->AmbientColor = { .5f,.4525f,.4f };
}

void Carol::Renderer::InitSsaoPass()
{
	mSsaoPass = std::make_unique<SsaoPass>();
	mSsaoPass->SetSampleCount(14);
	mSsaoPass->SetBlurRadius(5);
	mSsaoPass->SetBlurCount(3);
}

void Carol::Renderer::InitSsgiPass()
{
	mSsgiPass = std::make_unique<SsgiPass>();
	mSsgiPass->SetSampleCount(14);
	mSsgiPass->SetNumSteps(16);
}

void Carol::Renderer::InitTaaPass()
{
	mTaaPass = std::make_unique<TaaPass>();
}

void Carol::Renderer::InitUtilsPass()
{
	mUtilsPass = std::make_unique<UtilsPass>();
}

void Carol::Renderer::InitToneMappingPass()
{
	mToneMappingPass = std::make_unique<ToneMappingPass>();
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
		auto eventHandle = CreateEventEx(nullptr, LPCSTR(nullptr), 0, EVENT_ALL_ACCESS);
		ThrowIfFailed(gFence->SetEventOnCompletion(gCpuFenceValue, eventHandle));

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

void Carol::Renderer::Draw()
{	
	gCommandAllocatorPool->DiscardAllocator(gCommandAllocator.Get(), gCpuFenceValue);
	gCommandAllocator = gCommandAllocatorPool->RequestAllocator(gGpuFenceValue);
	ThrowIfFailed(gGraphicsCommandList->Reset(gCommandAllocator.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = {gDescriptorManager->GetResourceDescriptorHeap()};
	gGraphicsCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	gGraphicsCommandList->SetGraphicsRootSignature(gRootSignature->Get());
	gGraphicsCommandList->SetComputeRootSignature(gRootSignature->Get());

	gGraphicsCommandList->SetGraphicsRootConstantBufferView(FRAME_CB, mFrameCBAddr);
	gGraphicsCommandList->SetComputeRootConstantBufferView(FRAME_CB, mFrameCBAddr);

	mMainLightShadowPass->Draw();
	mCullPass->Draw();
	
	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(OPAQUE_MESH_START + i);
		mGeometryPass->SetIndirectCommandBuffer(type, mCullPass->GetIndirectCommandBuffer(type));
	}

	for (int i = 0; i < TRANSPARENT_MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(TRANSPARENT_MESH_START + i);
		mOitppllPass->SetIndirectCommandBuffer(type, mCullPass->GetIndirectCommandBuffer(type));
	}

	// Deferred shading for opaque meshes

	if (gModelManager->IsAnyOpaqueMeshes())
	{
		mGeometryPass->Draw();
	}

	mSsaoPass->Draw();
	mShadePass->Draw();
	// mSsgiPass->Draw();

	// Post process
	mToneMappingPass->Draw();
	mTaaPass->Draw();

	// Forward shading for transparent meshes
	if (gModelManager->IsAnyTransparentMeshes())
	{
		mOitppllPass->Draw();
	}

	// Display to screen
	mDisplayPass->Draw();
	
	ThrowIfFailed(gGraphicsCommandList->Close());
	std::vector<ID3D12CommandList*> cmdLists{ gGraphicsCommandList.Get()};
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
		float dx = DirectX::XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = DirectX::XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

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

		std::string fpsStr = std::to_string(fps);
		std::string mspfStr = std::to_string(mspf);

		std::string windowText = mMainWndCaption +
			"    fps: " + fpsStr +
			"   mspf: " + mspfStr;

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

	gModelManager->Update(mTimer.get(), gCpuFenceValue, gGpuFenceValue);
	mCamera->UpdateViewMatrix();
	mMainLightShadowPass->Update(dynamic_cast<PerspectiveCamera*>(mCamera.get()), .4f);

	DirectX::XMFLOAT4X4 jitteredProj4x4f = mCamera->GetProj4x4f();
	mTaaPass->GetHalton(jitteredProj4x4f._31, jitteredProj4x4f._32);

	DirectX::XMMATRIX view = mCamera->GetView();
	DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(nullptr, view);
	DirectX::XMMATRIX jitteredProj = DirectX::XMLoadFloat4x4(&jitteredProj4x4f);
	DirectX::XMMATRIX invJitteredProj = DirectX::XMMatrixInverse(nullptr, jitteredProj);
	DirectX::XMMATRIX viewJitteredProj = DirectX::XMMatrixMultiply(view, jitteredProj);
	DirectX::XMMATRIX invViewJitteredProj = DirectX::XMMatrixInverse(nullptr, viewJitteredProj);
		
	DirectX::XMStoreFloat4x4(&mFrameConstants->View, DirectX::XMMatrixTranspose(view));
	DirectX::XMStoreFloat4x4(&mFrameConstants->InvView, DirectX::XMMatrixTranspose(invView));
	DirectX::XMStoreFloat4x4(&mFrameConstants->Proj, DirectX::XMMatrixTranspose(jitteredProj));
	DirectX::XMStoreFloat4x4(&mFrameConstants->InvProj, DirectX::XMMatrixTranspose(invJitteredProj));
	DirectX::XMStoreFloat4x4(&mFrameConstants->ViewProj, DirectX::XMMatrixTranspose(viewJitteredProj));
	DirectX::XMStoreFloat4x4(&mFrameConstants->InvViewProj, DirectX::XMMatrixTranspose(invViewJitteredProj));

	mCullPass->Update(DirectX::XMLoadFloat4x4(&mFrameConstants->ViewProj), DirectX::XMLoadFloat4x4(&mFrameConstants->HistViewProj), DirectX::XMLoadFloat3(&mFrameConstants->EyePosW));

	DirectX::XMMATRIX veloProj = mCamera->GetProj();
	DirectX::XMMATRIX veloViewProj = DirectX::XMMatrixMultiply(view, veloProj);

	mFrameConstants->HistViewProj = mFrameConstants->VeloViewProj;
	DirectX::XMStoreFloat4x4(&mFrameConstants->VeloViewProj, DirectX::XMMatrixTranspose(veloViewProj));
	
	mFrameConstants->EyePosW = mCamera->GetPosition3f();
	mFrameConstants->NearZ = dynamic_cast<PerspectiveCamera*>(mCamera.get())->GetNearZ();
	mFrameConstants->FarZ = dynamic_cast<PerspectiveCamera*>(mCamera.get())->GetFarZ();
	
	mFrameConstants->NumMainLights = mMainLightShadowPass->GetSplitLevel();
	for (int i = 0; i < mFrameConstants->NumMainLights; ++i)
	{
		mFrameConstants->MainLights[i] = mMainLightShadowPass->GetLight(i);
		mFrameConstants->MainLightSplitZ[i] = mMainLightShadowPass->GetSplitZ(i);
	}

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

	mFrameConstants->RenderTargetSize = { float(mClientWidth),float(mClientHeight) };
	mFrameConstants->InvRenderTargetSize = { 1.0f / mClientWidth, 1.0f / mClientHeight };

	mTimer->Start();
	dynamic_cast<PerspectiveCamera*>(mCamera.get())->SetLens(0.25f * DirectX::XM_PI, AspectRatio(), 1.0f, 1000.0f);

	mCullPass->OnResize(width, height);
	mDisplayPass->OnResize(width, height);
	mGeometryPass->OnResize(width, height);
	mOitppllPass->OnResize(width, height);
	mShadePass->OnResize(width, height);
	mSsaoPass->OnResize(width, height);
	mSsgiPass->OnResize(width, height);
	mTaaPass->OnResize(width, height);
	mToneMappingPass->OnResize(width, height);

	mCullPass->SetDepthMap(mDisplayPass->GetDepthStencilMap());
	mGeometryPass->SetDepthStencilMap(mDisplayPass->GetDepthStencilMap());

	// Main light
	for (int i = 0; i < mMainLightShadowPass->GetSplitLevel(); ++i)
	{
		mFrameConstants->MainLightShadowMapIdx[i] = mMainLightShadowPass->GetShadowSrvIdx(i);
	}

	// Display
	mFrameConstants->RWFrameMapIdx = mDisplayPass->GetFrameMapUavIdx();
	mFrameConstants->FrameMapIdx = mDisplayPass->GetFrameMapSrvIdx();
	mFrameConstants->RWHistMapIdx = mDisplayPass->GetHistMapUavIdx();
	mFrameConstants->HistMapIdx = mDisplayPass->GetHistMapSrvIdx();
	mFrameConstants->DepthStencilMapIdx = mDisplayPass->GetDepthStencilSrvIdx();

	// G-Buffer
	mFrameConstants->DiffuseRougnessMapIdx = mGeometryPass->GetDiffuseRoughnessMapSrvIdx();
	mFrameConstants->EmissiveMetallicMapIdx = mGeometryPass->GetEmissiveMetallicMapSrvIdx();
	mFrameConstants->NormalMapIdx = mGeometryPass->GetNormalMapSrvIdx();
	mFrameConstants->VelocityMapIdx = mGeometryPass->GetVelocityMapSrvIdx();

	// OITPPLL
	mFrameConstants->RWOitppllBufferIdx = mOitppllPass->GetPpllUavIdx();
	mFrameConstants->RWOitppllStartOffsetBufferIdx = mOitppllPass->GetStartOffsetUavIdx();
	mFrameConstants->RWOitppllCounterIdx = mOitppllPass->GetCounterUavIdx();
	mFrameConstants->OitppllBufferIdx = mOitppllPass->GetPpllSrvIdx();
	mFrameConstants->OitppllStartOffsetBufferIdx = mOitppllPass->GetStartOffsetSrvIdx();
	
	// SSAO
	mFrameConstants->RWAmbientMapIdx = mSsaoPass->GetSsaoUavIdx();
	mFrameConstants->AmbientMapIdx = mSsaoPass->GetSsaoSrvIdx();

	// SSGI
	mFrameConstants->RWSceneColorIdx = mSsgiPass->GetSceneColorUavIdx();
	mFrameConstants->SceneColorIdx = mSsgiPass->GetSceneColorSrvIdx();
	mFrameConstants->RWSsgiHiZMapIdx = mSsgiPass->GetSsgiHiZUavIdx();
	mFrameConstants->SsgiHiZMapIdx = mSsgiPass->GetSsgiHiZSrvIdx();
	mFrameConstants->RWSsgiMapIdx = mSsgiPass->GetSsgiUavIdx();
	mFrameConstants->SsgiMapIdx = mSsgiPass->GetSsgiSrvIdx();

	// Utils
	mFrameConstants->RandVecMapIdx = mUtilsPass->GetRandVecSrvIdx();
}

void Carol::Renderer::LoadModel(std::string_view path, std::string_view textureDir, std::string_view modelName, DirectX::XMMATRIX world, bool isSkinned)
{
	gCommandAllocatorPool->DiscardAllocator(gCommandAllocator.Get(), gCpuFenceValue);
	gCommandAllocator = gCommandAllocatorPool->RequestAllocator(gGpuFenceValue);
	ThrowIfFailed(gGraphicsCommandList->Reset(gCommandAllocator.Get(), nullptr));

	gModelManager->LoadModel(
		modelName,
		path,
		textureDir,
		isSkinned);
	gModelManager->SetWorld(modelName, world);

	gGraphicsCommandList->Close();
	std::vector<ID3D12CommandList*> cmdLists = { gGraphicsCommandList.Get() };
	gCommandQueue->ExecuteCommandLists(1, cmdLists.data());

	++gCpuFenceValue;
	ThrowIfFailed(gCommandQueue->Signal(gFence.Get(), gCpuFenceValue));
}

void Carol::Renderer::UnloadModel(std::string_view modelName)
{
	gModelManager->UnloadModel(modelName);
}

std::vector<std::string_view> Carol::Renderer::GetAnimationNames(std::string_view modelName)
{
	return gModelManager->GetAnimationClips(modelName);
}

void Carol::Renderer::SetAnimation(std::string_view modelName, std::string_view animationName)
{
	gModelManager->SetAnimationClip(modelName, animationName);
}

std::vector<std::string_view> Carol::Renderer::GetModelNames()
{
	return gModelManager->GetModelNames();
}
