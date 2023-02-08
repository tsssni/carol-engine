#include <renderer.h>
#include <render_pass.h>
#include <dx12.h>
#include <scene.h>
#include <utils.h>
#include <DirectXColors.h>

namespace Carol {
	using std::vector;
	using std::unique_ptr;
	using std::wstring;
	using std::wstring_view;
	using std::make_unique;
	using namespace DirectX;

	D3D12_RASTERIZER_DESC gCullDisabledState;

	D3D12_DEPTH_STENCIL_DESC gDepthLessEqualState;
	D3D12_DEPTH_STENCIL_DESC gDepthDisabledState;

	D3D12_BLEND_DESC gAlphaBlendState;
}

Carol::Renderer::Renderer(HWND hWnd, uint32_t width, uint32_t height)
	:BaseRenderer(hWnd)
{
	ThrowIfFailed(mInitCommandAllocator->Reset());
	ThrowIfFailed(mCommandList->Reset(mInitCommandAllocator.Get(), nullptr));
	
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
}

void Carol::Renderer::InitConstants()
{
	mFrameConstants = make_unique<FrameConstants>();
	mFrameCBAllocator = make_unique<FastConstantBufferAllocator>(gNumFrame, sizeof(FrameConstants), mDevice.Get(), mHeapManager->GetUploadBuffersHeap(), mDescriptorManager.get());
}

void Carol::Renderer::InitPipelineStates()
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

void Carol::Renderer::InitFrame()
{
	mFramePass = make_unique<FramePass>(
		mDevice.Get(),
		mHeapManager->GetDefaultBuffersHeap(),
		mDescriptorManager.get(),
		mScene.get());
}

void Carol::Renderer::InitSsao()
{
	mSsaoPass = make_unique<SsaoPass>(
		mDevice.Get(),
		mCommandList.Get(),
		mHeapManager->GetDefaultBuffersHeap(),
		mHeapManager->GetUploadBuffersHeap(),
		mDescriptorManager.get());
}

void Carol::Renderer::InitNormal()
{
	mNormalPass = make_unique<NormalPass>(
		mDevice.Get(),
		mFramePass->GetFrameDsvFormat());
}

void Carol::Renderer::InitTaa()
{
	mTaaPass = make_unique<TaaPass>(
		mDevice.Get(),
		mFramePass->GetFrameFormat(),
		mFramePass->GetFrameDsvFormat());
}

void Carol::Renderer::InitMainLight()
{
	Light light = {};
	light.Direction = { 0.8f,-1.0f,1.0f };
	light.Strength = { 0.8f,0.8f,0.8f };
	mMainLightShadowPass = make_unique<CascadedShadowPass>(
		mDevice.Get(),
		mHeapManager->GetDefaultBuffersHeap(),
		mDescriptorManager.get(),
		mScene.get(),
		light);
}

void Carol::Renderer::InitScene()
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

void Carol::Renderer::UpdateFrameCB()
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
	mFrameConstants->NearZ = dynamic_cast<PerspectiveCamera*>(mCamera.get())->GetNearZ();
	mFrameConstants->FarZ = dynamic_cast<PerspectiveCamera*>(mCamera.get())->GetFarZ();
	
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
	mFrameCBAddr = mFrameCBAllocator->Allocate(mFrameConstants.get());
}

void Carol::Renderer::ReleaseIntermediateBuffers()
{
	mScene->ReleaseIntermediateBuffers();
	mSsaoPass->ReleaseIntermediateBuffers();
}

void Carol::Renderer::Draw()
{	
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
	mGpuFence[gCurrFrame] = ++mCpuFence;
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCpuFence));

	gCurrFrame = (gCurrFrame + 1) % gNumFrame;
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

void Carol::Renderer::Update()
{
	OnKeyboardInput();

	if (mGpuFence[gCurrFrame] != 0 && mFence->GetCompletedValue() < mGpuFence[gCurrFrame])
	{
		HANDLE eventHandle = CreateEventEx(nullptr, LPCWSTR(nullptr), 0, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mGpuFence[gCurrFrame], eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	ThrowIfFailed(mFrameAllocator[gCurrFrame]->Reset());
	ThrowIfFailed(mCommandList->Reset(mFrameAllocator[gCurrFrame].Get(), nullptr));
	
	mDescriptorManager->DelayedDelete();
	mHeapManager->DelayedDelete();

	mCamera->UpdateViewMatrix();
	mMainLightShadowPass->Update(dynamic_cast<PerspectiveCamera*>(mCamera.get()), 0.85f);
	UpdateFrameCB();

	mScene->Update(mTimer.get());
	mFramePass->Update();
}

void Carol::Renderer::OnResize(uint32_t width, uint32_t height, bool init)
{
	if (!init)
	{
		ThrowIfFailed(mInitCommandAllocator->Reset());
		ThrowIfFailed(mCommandList->Reset(mInitCommandAllocator.Get(), nullptr));
	}

	ID3D12Device* device = mDevice.Get();
	Heap* heap = mHeapManager->GetDefaultBuffersHeap();
	DescriptorManager* descriptorManager = mDescriptorManager.get();

	BaseRenderer::OnResize(width, height, init);
	mFramePass->OnResize(width, height, device, heap, descriptorManager);
	mNormalPass->OnResize(width, height, device, heap, descriptorManager);
	mSsaoPass->OnResize(width, height, device, heap, descriptorManager);
	mTaaPass->OnResize(width, height, device, heap, descriptorManager);

	mNormalPass->SetFrameDsv(mFramePass->GetFrameDsv());
	mTaaPass->SetFrameDsv(mFramePass->GetFrameDsv());

	mFrameConstants->MeshCBIdx = mScene->GetMeshCBIdx();
	mFrameConstants->CommandBufferIdx = mScene->GetCommandBufferIdx();
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

	mCommandList->Close();
	vector<ID3D12CommandList*> cmdLists{ mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(1, cmdLists.data());
	FlushCommandQueue();
}

void Carol::Renderer::LoadModel(wstring_view path, wstring_view textureDir, wstring_view modelName, DirectX::XMMATRIX world, bool isSkinned)
{
	FlushCommandQueue();
	ThrowIfFailed(mInitCommandAllocator->Reset());
	ThrowIfFailed(mCommandList->Reset(mInitCommandAllocator.Get(), nullptr));

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

	mCommandList->Close();
	vector<ID3D12CommandList*> cmdLists = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(1, cmdLists.data());
	FlushCommandQueue();
	
	mScene->ReleaseIntermediateBuffers(modelName);
}

void Carol::Renderer::UnloadModel(wstring_view modelName)
{
	FlushCommandQueue();
	mScene->UnloadModel(modelName);
}

Carol::vector<Carol::wstring_view> Carol::Renderer::GetAnimationNames(wstring_view modelName)
{
	return mScene->GetAnimationClips(modelName);
}

void Carol::Renderer::SetAnimation(wstring_view modelName, wstring_view animationName)
{
	mScene->SetAnimationClip(modelName, animationName);
}

Carol::vector<Carol::wstring_view> Carol::Renderer::GetModelNames()
{
	return mScene->GetModelNames();
}
