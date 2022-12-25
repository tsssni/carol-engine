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
#include <scene/assimp.h>
#include <scene/texture.h>
#include <scene/timer.h>
#include <scene/camera.h>
#include <scene/skinned_data.h>
#include <utils/bitset.h>
#include <utils/buddy.h>
#include <utils/common.h>
#include <DirectXColors.h>

namespace Carol {
	using std::vector;
	using std::unique_ptr;
	using std::make_unique;
	using namespace DirectX;
}

Carol::unique_ptr<Carol::CircularHeap> Carol::FramePass::FrameCBHeap = nullptr;
Carol::unique_ptr<Carol::CircularHeap> Carol::SsaoPass::SsaoCBHeap = nullptr;
Carol::unique_ptr<Carol::CircularHeap> Carol::MeshesPass::MeshCBHeap = nullptr;
Carol::unique_ptr<Carol::CircularHeap> Carol::MeshesPass::SkinnedCBHeap = nullptr;

Carol::Renderer::Renderer(HWND hWnd, uint32_t width, uint32_t height)
	:BaseRenderer(hWnd, width, height)
{
	ThrowIfFailed(mInitCommandAllocator->Reset());
	ThrowIfFailed(mCommandList->Reset(mInitCommandAllocator.Get(), nullptr));
	
	InitPSOs();
	InitFrame();
	InitSsao();
	InitNormal();
	InitTaa();
	InitMainLight();
	InitOitppll();
	InitMeshes();

	mCommandList->Close();
	vector<ID3D12CommandList*> cmdLists{ mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(1, cmdLists.data());
	FlushCommandQueue();
	ReleaseIntermediateBuffers();
}

void Carol::Renderer::InitPSOs()
{
	BaseRenderer::InitPSOs();

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "BONEINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	mBasePsoDesc.InputLayout = { mInputLayout.data(),(uint32_t)mInputLayout.size() };
	mBasePsoDesc.pRootSignature = mRootSignature->Get();
	mBasePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    mBasePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	mBasePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    mBasePsoDesc.SampleMask = UINT_MAX;
    mBasePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    mBasePsoDesc.NumRenderTargets = 1;
    mBasePsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    mBasePsoDesc.SampleDesc.Count = 1;
    mBasePsoDesc.SampleDesc.Quality = 0;
    mBasePsoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	mGlobalResources->BasePsoDesc = &mBasePsoDesc;
}

void Carol::Renderer::InitFrame()
{
	mFrame = make_unique<FramePass>(mGlobalResources.get());
	FramePass::InitFrameCBHeap(mDevice.Get(), mCommandList.Get());
	mGlobalResources->Frame = mFrame.get();
}

void Carol::Renderer::InitSsao()
{
	mSsao = make_unique<SsaoPass>(mGlobalResources.get());
	SsaoPass::InitSsaoCBHeap(mDevice.Get(), mCommandList.Get());
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
	MeshesPass::InitMeshCBHeap(mDevice.Get(), mCommandList.Get());
	MeshesPass::InitSkinnedCBHeap(mDevice.Get(), mCommandList.Get());

	mTexManager = make_unique<TextureManager>(mDevice.Get(), mCommandList.Get(), mTexturesHeap.get(), mUploadBuffersHeap.get(), mRootSignature.get(), mCbvSrvUavAllocator.get(), mNumFrame);
	mGlobalResources->TexManager = mTexManager.get();
	mGlobalResources->Meshes = mMeshes.get();
	
	mMeshes->LoadGround();
	mMeshes->LoadSkyBox();
}

void Carol::Renderer::ReleaseIntermediateBuffers()
{
	mSsao->ReleaseIntermediateBuffers();
	mOitppll->ReleaseIntermediateBuffers();
	mMeshes->ReleaseIntermediateBuffers();
}

void Carol::Renderer::CopyDescriptors()
{
	mFrame->CopyDescriptors();
	mMainLight->CopyDescriptors();
	mNormal->CopyDescriptors();
	mSsao->CopyDescriptors();
	mOitppll->CopyDescriptors();
	mTaa->CopyDescriptors();
}

void Carol::Renderer::Draw()
{	
	ID3D12DescriptorHeap* descriptorHeaps[] = {mCbvSrvUavAllocator->GetGpuDescriptorHeap()};
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	
	mCommandList->SetGraphicsRootSignature(mRootSignature->Get());
	mCommandList->SetGraphicsRootConstantBufferView(RootSignature::ROOT_SIGNATURE_FRAME_CB, mFrame->GetFrameAddress());
	
	SetTextures();
	mMainLight->Draw();
	mNormal->Draw();
	mFrame->Draw();
	mSsao->Draw();
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
	
	mTexManager->ClearGpuTextures(mCurrFrame);
	CopyDescriptors();

	mMainLight->Update();
	mSsao->Update();
	mMeshes->Update();
	mFrame->Update();

	mTexManager->AllocateGpuTextures(mCurrFrame);
}

void Carol::Renderer::SetTextures()
{
	if (mTexManager->GetNumTex1D())
	{
		mCommandList->SetGraphicsRootDescriptorTable(RootSignature::ROOT_SIGNATURE_TEX1D, mTexManager->GetTex1DHandle(mCurrFrame));
	}

	if (mTexManager->GetNumTex2D())
	{
		mCommandList->SetGraphicsRootDescriptorTable(RootSignature::ROOT_SIGNATURE_TEX2D, mTexManager->GetTex2DHandle(mCurrFrame));
	}

	if (mTexManager->GetNumTex3D())
	{
		mCommandList->SetGraphicsRootDescriptorTable(RootSignature::ROOT_SIGNATURE_TEX3D, mTexManager->GetTex3DHandle(mCurrFrame));
	}

	if (mTexManager->GetNumTexCube())
	{
		mCommandList->SetGraphicsRootDescriptorTable(RootSignature::ROOT_SIGNATURE_TEXCUBE, mTexManager->GetTexCubeHandle(mCurrFrame));
	}
}

void Carol::Renderer::OnResize(uint32_t width, uint32_t height)
{
	BaseRenderer::OnResize(width, height);
	mFrame->OnResize();
	mNormal->OnResize();
	mSsao->OnResize();
	mTaa->OnResize();
	mOitppll->OnResize();
}

void Carol::Renderer::LoadModel(wstring path, wstring textureDir, wstring modelName, DirectX::XMMATRIX world, bool isSkinned, bool isTransparent)
{
	ThrowIfFailed(mInitCommandAllocator->Reset());
	ThrowIfFailed(mCommandList->Reset(mInitCommandAllocator.Get(), nullptr));

	mMeshes->LoadModel(modelName, path, textureDir, isSkinned);
	mMeshes->SetWorld(modelName, world);

	mCommandList->Close();
	vector<ID3D12CommandList*> cmdLists = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(1, cmdLists.data());
	FlushCommandQueue();
	
	mMeshes->ReleaseIntermediateBuffers(modelName);
}

void Carol::Renderer::UnloadModel(wstring modelName)
{
	FlushCommandQueue();
	mMeshes->UnloadModel(modelName);
}

Carol::vector<wstring> Carol::Renderer::GetAnimationNames(wstring modelName)
{
	return mMeshes->GetAnimationClips(modelName);
}

void Carol::Renderer::SetAnimation(wstring modelName, wstring animationName)
{
	mMeshes->SetAnimationClip(modelName, animationName);
}

Carol::vector<wstring> Carol::Renderer::GetModelNames()
{
	return mMeshes->GetModels();
}
