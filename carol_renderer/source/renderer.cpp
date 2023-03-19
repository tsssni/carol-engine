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
	using Microsoft::WRL::ComPtr;
	using namespace DirectX;

	D3D12_RASTERIZER_DESC gCullDisabledState;

	D3D12_DEPTH_STENCIL_DESC gDepthLessEqualState;
	D3D12_DEPTH_STENCIL_DESC gDepthDisabledState;

	D3D12_BLEND_DESC gAlphaBlendState;
}

Carol::Renderer::Renderer(HWND hWnd, uint32_t width, uint32_t height)
	:BaseRenderer(hWnd)
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

void Carol::Renderer::InitConstants()
{
	mFrameConstants = make_unique<FrameConstants>();
	mFrameCBAllocator = make_unique<FastConstantBufferAllocator>(1024, sizeof(FrameConstants), mDevice.Get(), mHeapManager->GetUploadBuffersHeap(), mDescriptorManager.get());
}

void Carol::Renderer::InitPipelineStates()
{
	gCullDisabledState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gCullDisabledState.CullMode = D3D12_CULL_MODE_NONE;

	gDepthDisabledState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	gDepthDisabledState.DepthEnable = false;
	gDepthDisabledState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

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
		mDevice.Get());
}

void Carol::Renderer::InitTaa()
{
	mTaaPass = make_unique<TaaPass>(
		mDevice.Get());
}

void Carol::Renderer::InitMainLight()
{
	Light light = {};
	light.Strength = { 1.f,1.f,1.f };
	XMStoreFloat3(&light.Direction, { .8f,-1.f,1.f });

	mMainLightShadowPass = make_unique<CascadedShadowPass>(
		mDevice.Get(),
		mHeapManager->GetDefaultBuffersHeap(),
		mDescriptorManager.get(),
		mScene.get(),
		light);

	mFrameConstants->AmbientColor = { .1f,.1f,.1f };

	mFrameConstants->NumPointLights = 4;

	for (int i = 0; i < mFrameConstants->NumPointLights; ++i)
	{
		auto& light = mFrameConstants->PointLights[i];
		light.Strength = { .5f,.5f,.4f };
		light.AttenuationQuadric = 1.f / 200.f;
		XMStoreFloat3(&light.Position, XMVector3Transform({ 0.f,25.f,-20.f }, XMMatrixRotationY(90.f * i)));
		XMStoreFloat3(&light.Direction, XMVector3Normalize(XMVector3Transform({ 0.f,-5.f,1.f }, XMMatrixRotationY(90.f * i))));
	}
}

void Carol::Renderer::InitScene()
{
	mScene = make_unique<Scene>(
		L"Test",
		mDevice.Get(),
		mHeapManager->GetDefaultBuffersHeap(),
		mHeapManager->GetUploadBuffersHeap(),
		mDescriptorManager.get());
	
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
		mFrameConstants->MainLights[i] = mMainLightShadowPass->GetLight(i);
	}

	for (int i = 0; i < mMainLightShadowPass->GetSplitLevel() + 1; ++i)
	{
		mFrameConstants->MainLightSplitZ[i] = mMainLightShadowPass->GetSplitZ(i);
	}

	mFrameConstants->MeshBufferIdx = mScene->GetMeshBufferIdx();
	mFrameConstants->CommandBufferIdx = mScene->GetCommandBufferIdx();
	mFrameConstants->RWFrameMapIdx = mDisplayPass->GetFrameMapUavIdx();
	mFrameCBAddr = mFrameCBAllocator->Allocate(mFrameConstants.get());
}

void Carol::Renderer::ReleaseIntermediateBuffers()
{
	mScene->ReleaseIntermediateBuffers();
	mSsaoPass->ReleaseIntermediateBuffers();
}

void Carol::Renderer::Draw()
{	
	mCommandAllocatorPool->DiscardAllocator(mCommandAllocator.Get(), mCpuFenceValue);
	mCommandAllocator = mCommandAllocatorPool->RequestAllocator(mGpuFenceValue);
	ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = {mDescriptorManager->GetResourceDescriptorHeap()};
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mFramePass->SetFrameMap(mDisplayPass->GetFrameMap());
	mTaaPass->SetFrameMap(mDisplayPass->GetFrameMap());

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
	mDisplayPass->Draw(mCommandList.Get());
	
	ThrowIfFailed(mCommandList->Close());
	vector<ID3D12CommandList*> cmdLists{ mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(1, cmdLists.data());

	mDisplayPass->Present();
	++mCpuFenceValue;
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCpuFenceValue));
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
	mGpuFenceValue = mFence->GetCompletedValue();

	mDescriptorManager->DelayedDelete(mCpuFenceValue, mGpuFenceValue);
	mHeapManager->DelayedDelete(mCpuFenceValue, mGpuFenceValue);

	mCamera->UpdateViewMatrix();
	mScene->Update(mTimer.get(), mCpuFenceValue, mGpuFenceValue);
	mMainLightShadowPass->Update(dynamic_cast<PerspectiveCamera*>(mCamera.get()), mCpuFenceValue, mGpuFenceValue, 0.85f);
	mFramePass->Update(mCpuFenceValue, mGpuFenceValue);

	UpdateFrameCB();
}

void Carol::Renderer::OnResize(uint32_t width, uint32_t height, bool init)
{
	ID3D12Device* device = mDevice.Get();
	Heap* heap = mHeapManager->GetDefaultBuffersHeap();
	DescriptorManager* descriptorManager = mDescriptorManager.get();

	BaseRenderer::OnResize(width, height, init);
	mFramePass->OnResize(width, height, device, heap, descriptorManager);
	mNormalPass->OnResize(width, height, device, heap, descriptorManager);
	mSsaoPass->OnResize(width, height, device, heap, descriptorManager);
	mTaaPass->OnResize(width, height, device, heap, descriptorManager);

	mFramePass->SetDepthStencilMap(mDisplayPass->GetDepthStencilMap());
	mNormalPass->SetDepthStencilMap(mDisplayPass->GetDepthStencilMap());
	mTaaPass->SetDepthStencilMap(mDisplayPass->GetDepthStencilMap());

	mFrameConstants->InstanceFrustumCulledMarkBufferIdx = mScene->GetInstanceFrustumCulledMarkBufferIdx();
	mFrameConstants->InstanceOcclusionCulledMarkBufferIdx = mScene->GetInstanceOcclusionCulledMarkBufferIdx();
	mFrameConstants->InstanceCulledMarkBufferIdx = mScene->GetInstanceCulledMarkBufferIdx();
	
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

void Carol::Renderer::UnloadModel(wstring_view modelName)
{
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
