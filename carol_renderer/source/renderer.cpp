#include <renderer.h>
#include <render_pass/global_resources.h>
#include <render_pass/display.h>
#include <render_pass/frame.h>
#include <render_pass/ssao.h>
#include <render_pass/normal.h>
#include <render_pass/taa.h>
#include <render_pass/shadow.h>
#include <render_pass/oitppll.h>
#include <render_pass/mesh.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/descriptor_allocator.h>
#include <dx12/root_signature.h>
#include <dx12/shader.h>
#include <scene/scene.h>
#include <scene/assimp.h>
#include <scene/texture.h>
#include <scene/timer.h>
#include <scene/camera.h>
#include <scene/skinned_data.h>
#include <utils/bitset.h>
#include <utils/buddy.h>
#include <utils/common.h>
#include <utils/d3dx12.h>
#include <DirectXColors.h>

namespace Carol {
	using std::vector;
	using std::unique_ptr;
	using std::make_unique;
	using namespace DirectX;
}

Carol::Renderer::Renderer(HWND hWnd, uint32_t width, uint32_t height)
	:BaseRenderer(hWnd, width, height), mFrameIdx(FRAME_IDX_COUNT, 0)
{
	ThrowIfFailed(mInitCommandAllocator->Reset());
	ThrowIfFailed(mCommandList->Reset(mInitCommandAllocator.Get(), nullptr));
	
	InitConstants();
	InitPSOs();
	InitFrame();
	InitNormal();
	InitMainLight();
	InitOitppll();
	InitSsao();
	InitTaa();
	InitMeshes();
	OnResize(width, height, true);
	ReleaseIntermediateBuffers();
}

void Carol::Renderer::InitConstants()
{
	mFrameCBHeap = make_unique<CircularHeap>(mDevice.Get(), mCommandList.Get(), true, 32, sizeof(FrameConstants));
	mFrameConstants = make_unique<FrameConstants>();
	mFrameCBAllocInfo = make_unique<HeapAllocInfo>();
}

void Carol::Renderer::InitPSOs()
{
	BaseRenderer::InitPSOs();

	mBaseGraphicsPsoDesc.pRootSignature = mRootSignature->Get();
	mBaseGraphicsPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	mBaseGraphicsPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	mBaseGraphicsPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	mBaseGraphicsPsoDesc.SampleMask = UINT_MAX;
	mBaseGraphicsPsoDesc.NodeMask = 0;
	mBaseGraphicsPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	mBaseGraphicsPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	mBaseGraphicsPsoDesc.NumRenderTargets = 1;
	mBaseGraphicsPsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	mBaseGraphicsPsoDesc.SampleDesc.Count = 1;
	mBaseGraphicsPsoDesc.SampleDesc.Quality = 0;
	mBaseGraphicsPsoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	mGlobalResources->BaseGraphicsPsoDesc = &mBaseGraphicsPsoDesc;

	mBaseComputePsoDesc.pRootSignature = mRootSignature->Get();
	mBaseComputePsoDesc.NodeMask = 0;
	mBaseComputePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	mGlobalResources->BaseComputePsoDesc = &mBaseComputePsoDesc;
}

void Carol::Renderer::InitFrame()
{
	mFrame = make_unique<FramePass>(mGlobalResources.get());
	mGlobalResources->Frame = mFrame.get();
}

void Carol::Renderer::InitSsao()
{
	mSsao = make_unique<SsaoPass>(mGlobalResources.get());
	mGlobalResources->Ssao = mSsao.get();
}

void Carol::Renderer::InitNormal()
{
	mNormal = make_unique<NormalPass>(mGlobalResources.get());
	mGlobalResources->Normal = mNormal.get();
}

void Carol::Renderer::InitTaa()
{
	mTaa = make_unique<TaaPass>(mGlobalResources.get());
	mGlobalResources->Taa = mTaa.get();
}

void Carol::Renderer::InitMainLight()
{
	Light light = {};
	light.Direction = { 0.8f,-1.0f,1.0f };
	light.Strength = { 0.8f,0.8f,0.8f };
	mMainLight = make_unique<ShadowPass>(mGlobalResources.get(), light, 2048, 2048);
	mGlobalResources->MainLight = mMainLight.get();
}

void Carol::Renderer::InitOitppll()
{
	mOitppll = make_unique<OitppllPass>(mGlobalResources.get());
	mGlobalResources->Oitppll = mOitppll.get();
}

void Carol::Renderer::InitMeshes()
{
	mMeshes = make_unique<MeshesPass>(mGlobalResources.get());
	mScene = make_unique<Scene>(L"Test", mDevice.Get(), mCommandList.Get(), mDefaultBuffersHeap.get(), mTexturesHeap.get(), mUploadBuffersHeap.get(), mCbvSrvUavAllocator.get());
	
	mScene->LoadGround();
	mScene->LoadSkyBox();

	mGlobalResources->Meshes = mMeshes.get();
	mGlobalResources->Scene = mScene.get();
}

void Carol::Renderer::UpdateFrameCB()
{
	mCamera->UpdateViewMatrix();

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
	mTaa->GetHalton(jitteredProj4x4f._31, jitteredProj4x4f._32);
	mTaa->SetHistViewProj(viewProj);

	XMMATRIX histViewProj = mTaa->GetHistViewProj();
	XMMATRIX jitteredProj = XMLoadFloat4x4(&jitteredProj4x4f);
	XMMATRIX jitteredViewProj = XMMatrixMultiply(view, jitteredProj);

	XMStoreFloat4x4(&mFrameConstants->HistViewProj, XMMatrixTranspose(histViewProj));
	XMStoreFloat4x4(&mFrameConstants->JitteredViewProj, XMMatrixTranspose(jitteredViewProj));

	mFrameConstants->EyePosW = mCamera->GetPosition3f();
	mFrameConstants->RenderTargetSize = { static_cast<float>(mClientWidth), static_cast<float>(mClientHeight) };
	mFrameConstants->InvRenderTargetSize = { 1.0f / mClientWidth, 1.0f / mClientHeight };
	mFrameConstants->NearZ = mCamera->GetNearZ();
	mFrameConstants->FarZ = mCamera->GetFarZ();
	
	mSsao->GetOffsetVectors(mFrameConstants->OffsetVectors);
	auto blurWeights = mSsao->CalcGaussWeights(2.5f);
	mFrameConstants->BlurWeights[0] = XMFLOAT4(&blurWeights[0]);
    mFrameConstants->BlurWeights[1] = XMFLOAT4(&blurWeights[4]);
    mFrameConstants->BlurWeights[2] = XMFLOAT4(&blurWeights[8]);

	mFrameConstants->Lights[0] = mMainLight->GetLight();

	mFrameCBHeap->DeleteResource(mFrameCBAllocInfo.get());
	mFrameCBHeap->CreateResource(mFrameCBAllocInfo.get());
	mFrameCBHeap->CopyData(mFrameCBAllocInfo.get(), mFrameConstants.get());
}

void Carol::Renderer::DelayedDelete()
{
	mScene->DelayedDelete(mCurrFrame);
	mFrameCBHeap->DelayedDelete(mCurrFrame);
	mDefaultBuffersHeap->DelayedDelete(mCurrFrame);
	mUploadBuffersHeap->DelayedDelete(mCurrFrame);
	mReadbackBuffersHeap->DelayedDelete(mCurrFrame);
	mTexturesHeap->DelayedDelete(mCurrFrame);
}

void Carol::Renderer::ReleaseIntermediateBuffers()
{
	mScene->ReleaseIntermediateBuffers();
	mSsao->ReleaseIntermediateBuffers();
}

void Carol::Renderer::Draw()
{	
	ID3D12DescriptorHeap* descriptorHeaps[] = {mCbvSrvUavAllocator->GetGpuDescriptorHeap()};
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature->Get());
	mCommandList->SetComputeRootSignature(mRootSignature->Get());

	mCommandList->SetGraphicsRootConstantBufferView(RootSignature::FRAME_CB, mFrameCBHeap->GetGPUVirtualAddress(mFrameCBAllocInfo.get()));
	mCommandList->SetGraphicsRoot32BitConstants(RootSignature::FRAME_CONSTANTS, FRAME_IDX_COUNT, mFrameIdx.data(), 0);
	mCommandList->SetComputeRootConstantBufferView(RootSignature::FRAME_CB, mFrameCBHeap->GetGPUVirtualAddress(mFrameCBAllocInfo.get()));
	mCommandList->SetComputeRoot32BitConstants(RootSignature::FRAME_CONSTANTS, FRAME_IDX_COUNT, mFrameIdx.data(), 0);

	mMainLight->Draw();
	mNormal->Draw();
	mSsao->Draw();
	mFrame->Draw();
	mTaa->Draw();
	
	mCommandList->Close();
	vector<ID3D12CommandList*> cmdLists{ mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(1, cmdLists.data());

	mDisplay->Present();
	mGpuFence[mCurrFrame] = ++mCpuFence;
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCpuFence));

	mCurrFrame = (mCurrFrame + 1) % mNumFrame;
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

	if (mGpuFence[mCurrFrame] != 0 && mFence->GetCompletedValue() < mGpuFence[mCurrFrame])
	{
		HANDLE eventHandle = CreateEventEx(nullptr, LPCWSTR(nullptr), 0, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mGpuFence[mCurrFrame], eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	ThrowIfFailed(mFrameAllocator[mCurrFrame]->Reset());
	ThrowIfFailed(mCommandList->Reset(mFrameAllocator[mCurrFrame].Get(), nullptr));
	
	DelayedDelete();
	mScene->Update(mCamera.get(), mTimer.get());
	mMeshes->Update();
	mMainLight->Update();
	mSsao->Update();
	mFrame->Update();
	UpdateFrameCB();
}

void Carol::Renderer::OnResize(uint32_t width, uint32_t height, bool init)
{
	if (!init)
	{
		ThrowIfFailed(mInitCommandAllocator->Reset());
		ThrowIfFailed(mCommandList->Reset(mInitCommandAllocator.Get(), nullptr));
	}

	BaseRenderer::OnResize(width, height);
	mFrame->OnResize();
	mNormal->OnResize();
	mSsao->OnResize();
	mTaa->OnResize();
	mOitppll->OnResize();

	mFrameIdx[FRAME_IDX] = mFrame->GetFrameSrvIdx();
	mFrameIdx[DEPTH_STENCIL_IDX] = mFrame->GetDepthStencilSrvIdx();
	mFrameIdx[NORMAL_IDX] = mNormal->GetNormalSrvIdx();
	mFrameIdx[SHADOW_IDX] = mMainLight->GetShadowSrvIdx();
	mFrameIdx[OIT_W_IDX] = mOitppll->GetPpllUavIdx();
	mFrameIdx[OIT_OFFSET_W_IDX] = mOitppll->GetOffsetUavIdx();
	mFrameIdx[OIT_COUNTER_IDX] = mOitppll->GetCounterUavIdx();
	mFrameIdx[OIT_R_IDX] = mOitppll->GetPpllSrvIdx();
	mFrameIdx[OIT_OFFSET_R_IDX] = mOitppll->GetOffsetSrvIdx();
	mFrameIdx[RAND_VEC_IDX] = mSsao->GetRandVecSrvIdx();
	mFrameIdx[AMBIENT_IDX] = mSsao->GetSsaoSrvIdx();
	mFrameIdx[VELOCITY_IDX] = mTaa->GetVeloctiySrvIdx();
	mFrameIdx[HIST_IDX] = mTaa->GetHistFrameSrvIdx();

	mCommandList->Close();
	vector<ID3D12CommandList*> cmdLists{ mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(1, cmdLists.data());
	FlushCommandQueue();
}

void Carol::Renderer::LoadModel(wstring path, wstring textureDir, wstring modelName, DirectX::XMMATRIX world, bool isSkinned)
{
	ThrowIfFailed(mInitCommandAllocator->Reset());
	ThrowIfFailed(mCommandList->Reset(mInitCommandAllocator.Get(), nullptr));

	mScene->LoadModel(modelName, path, textureDir, isSkinned);
	mScene->SetWorld(modelName, world);

	mCommandList->Close();
	vector<ID3D12CommandList*> cmdLists = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(1, cmdLists.data());
	FlushCommandQueue();
	
	mScene->ReleaseIntermediateBuffers(modelName);
}

void Carol::Renderer::UnloadModel(wstring modelName)
{
	FlushCommandQueue();
	mScene->UnloadModel(modelName);
}

Carol::vector<wstring> Carol::Renderer::GetAnimationNames(wstring modelName)
{
	return mScene->GetAnimationClips(modelName);
}

void Carol::Renderer::SetAnimation(wstring modelName, wstring animationName)
{
	mScene->SetAnimationClip(modelName, animationName);
}

Carol::vector<wstring> Carol::Renderer::GetModelNames()
{
	return mScene->GetModelNames();
}
