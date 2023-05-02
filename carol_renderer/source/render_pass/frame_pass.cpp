#include <carol.h>
#include <DirectXColors.h>
#include <string_view>
#include <span>

namespace Carol
{
	using std::vector;
	using std::wstring;
	using std::wstring_view;
	using std::unique_ptr;
	using std::make_unique;
	using std::span;
	using Microsoft::WRL::ComPtr;
	using namespace DirectX;
}

Carol::FramePass::FramePass(
	DXGI_FORMAT frameFormat,
	DXGI_FORMAT depthStencilFormat,
	DXGI_FORMAT hiZFormat)
	:mFrameFormat(frameFormat),
	mDepthStencilFormat(depthStencilFormat)
{
	InitPSOs();

	mCullPass = make_unique<CullPass>(
		0,
		0.f,
		0.f,
		depthStencilFormat);
}

void Carol::FramePass::Draw()
{
	gGraphicsCommandList->RSSetViewports(1, &mViewport);
	gGraphicsCommandList->RSSetScissorRects(1, &mScissorRect);

	mFrameMap->Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);
	gGraphicsCommandList->ClearRenderTargetView(mFrameMap->GetRtv(), Colors::Gray, 0, nullptr);
	gGraphicsCommandList->ClearDepthStencilView(mDepthStencilMap->GetDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);

	DrawOpaque();
	DrawTransparent();
}

void Carol::FramePass::Update(XMMATRIX viewProj, XMMATRIX histViewProj, XMVECTOR eyePos)
{
	mCullPass->Update(viewProj, histViewProj, eyePos);
}

void Carol::FramePass::Cull()
{
	mCullPass->Draw();
}

void Carol::FramePass::SetFrameMap(ColorBuffer* frameMap)
{
	mFrameMap = frameMap;
}

void Carol::FramePass::SetDepthStencilMap(ColorBuffer* depthStencilMap)
{
	mDepthStencilMap = depthStencilMap;
	mCullPass->SetDepthMap(depthStencilMap);
}

const Carol::StructuredBuffer* Carol::FramePass::GetIndirectCommandBuffer(MeshType type)const
{
	return mCullPass->GetIndirectCommandBuffer(type);
}

uint32_t Carol::FramePass::GetPpllUavIdx()const
{
	return mOitppllBuffer->GetGpuUavIdx();
}

uint32_t Carol::FramePass::GetOffsetUavIdx()const
{
	return mStartOffsetBuffer->GetGpuUavIdx();
}

uint32_t Carol::FramePass::GetCounterUavIdx()const
{
	return mCounterBuffer->GetGpuUavIdx();
}

uint32_t Carol::FramePass::GetPpllSrvIdx()const
{
	return mOitppllBuffer->GetGpuSrvIdx();
}

uint32_t Carol::FramePass::GetOffsetSrvIdx()const
{
	return mStartOffsetBuffer->GetGpuSrvIdx();
}

void Carol::FramePass::InitPSOs()
{
	mBlinnPhongStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mBlinnPhongStaticMeshPSO->SetRootSignature(sRootSignature.get());
	mBlinnPhongStaticMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
	mBlinnPhongStaticMeshPSO->SetAS(gCullAS.get());
	mBlinnPhongStaticMeshPSO->SetMS(gMeshStaticMS.get());
	mBlinnPhongStaticMeshPSO->SetPS(gBlinnPhongPS.get());
	mBlinnPhongStaticMeshPSO->Finalize();

	mBlinnPhongSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mBlinnPhongSkinnedMeshPSO->SetRootSignature(sRootSignature.get());
	mBlinnPhongSkinnedMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
	mBlinnPhongSkinnedMeshPSO->SetAS(gCullAS.get());
	mBlinnPhongSkinnedMeshPSO->SetMS(gMeshSkinnedMS.get());
	mBlinnPhongSkinnedMeshPSO->SetPS(gBlinnPhongPS.get());
	mBlinnPhongSkinnedMeshPSO->Finalize();

	mPBRStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mPBRStaticMeshPSO->SetRootSignature(sRootSignature.get());
	mPBRStaticMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
	mPBRStaticMeshPSO->SetAS(gCullAS.get());
	mPBRStaticMeshPSO->SetMS(gMeshStaticMS.get());
	mPBRStaticMeshPSO->SetPS(gPBRPS.get());
	mPBRStaticMeshPSO->Finalize();

	mPBRSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mPBRSkinnedMeshPSO->SetRootSignature(sRootSignature.get());
	mPBRSkinnedMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
	mPBRSkinnedMeshPSO->SetAS(gCullAS.get());
	mPBRSkinnedMeshPSO->SetMS(gMeshSkinnedMS.get());
	mPBRSkinnedMeshPSO->SetPS(gPBRPS.get());
	mPBRSkinnedMeshPSO->Finalize();

	mBlinnPhongStaticOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mBlinnPhongStaticOitppllMeshPSO->SetRootSignature(sRootSignature.get());
	mBlinnPhongStaticOitppllMeshPSO->SetRasterizerState(gCullDisabledState.get());
	mBlinnPhongStaticOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState.get());
	mBlinnPhongStaticOitppllMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
	mBlinnPhongStaticOitppllMeshPSO->SetAS(gCullAS.get());
	mBlinnPhongStaticOitppllMeshPSO->SetMS(gMeshStaticMS.get());
	mBlinnPhongStaticOitppllMeshPSO->SetPS(gBlinnPhongOitppllPS.get());
	mBlinnPhongStaticOitppllMeshPSO->Finalize();

	mBlinnPhongSkinnedOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mBlinnPhongSkinnedOitppllMeshPSO->SetRootSignature(sRootSignature.get());
	mBlinnPhongSkinnedOitppllMeshPSO->SetRasterizerState(gCullDisabledState.get());
	mBlinnPhongSkinnedOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState.get());
	mBlinnPhongSkinnedOitppllMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
	mBlinnPhongSkinnedOitppllMeshPSO->SetAS(gCullAS.get());
	mBlinnPhongSkinnedOitppllMeshPSO->SetMS(gMeshSkinnedMS.get());
	mBlinnPhongSkinnedOitppllMeshPSO->SetPS(gBlinnPhongOitppllPS.get());
	mBlinnPhongSkinnedOitppllMeshPSO->Finalize();

	mPBRStaticOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mPBRStaticOitppllMeshPSO->SetRootSignature(sRootSignature.get());
	mPBRStaticOitppllMeshPSO->SetRasterizerState(gCullDisabledState.get());
	mPBRStaticOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState.get());
	mPBRStaticOitppllMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
	mPBRStaticOitppllMeshPSO->SetAS(gCullAS.get());
	mPBRStaticOitppllMeshPSO->SetMS(gMeshStaticMS.get());
	mPBRStaticOitppllMeshPSO->SetPS(gPBROitppllPS.get());
	mPBRStaticOitppllMeshPSO->Finalize();

	mPBRSkinnedOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mPBRSkinnedOitppllMeshPSO->SetRootSignature(sRootSignature.get());
	mPBRSkinnedOitppllMeshPSO->SetRasterizerState(gCullDisabledState.get());
	mPBRSkinnedOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState.get());
	mPBRSkinnedOitppllMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
	mPBRSkinnedOitppllMeshPSO->SetAS(gCullAS.get());
	mPBRSkinnedOitppllMeshPSO->SetMS(gMeshSkinnedMS.get());
	mPBRSkinnedOitppllMeshPSO->SetPS(gPBROitppllPS.get());
	mPBRSkinnedOitppllMeshPSO->Finalize();

	mDrawOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mDrawOitppllMeshPSO->SetRootSignature(sRootSignature.get());
	mDrawOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState.get());
	mDrawOitppllMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
	mDrawOitppllMeshPSO->SetBlendState(gAlphaBlendState.get());
	mDrawOitppllMeshPSO->SetRenderTargetFormat(mFrameFormat);
	mDrawOitppllMeshPSO->SetMS(gScreenMS.get());
	mDrawOitppllMeshPSO->SetPS(gDrawOitppllPS.get());
	mDrawOitppllMeshPSO->Finalize();
	
	mSkyBoxMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mSkyBoxMeshPSO->SetRootSignature(sRootSignature.get());
	mSkyBoxMeshPSO->SetDepthStencilState(gDepthLessEqualState.get());
	mSkyBoxMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
	mSkyBoxMeshPSO->SetMS(gSkyBoxMS.get());
	mSkyBoxMeshPSO->SetPS(gSkyBoxPS.get());
	mSkyBoxMeshPSO->Finalize();
}

void Carol::FramePass::InitBuffers()
{
	mOitppllBuffer = make_unique<StructuredBuffer>(
		mWidth * mHeight * 16,
		sizeof(OitppllNode),
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mStartOffsetBuffer = make_unique<RawBuffer>(
		mWidth * mHeight * sizeof(uint32_t),
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mCounterBuffer = make_unique<RawBuffer>(
		sizeof(uint32_t),
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mCullPass->OnResize(mWidth, mHeight);
}

void Carol::FramePass::DrawOpaque()
{
	gGraphicsCommandList->OMSetRenderTargets(1, GetRvaluePtr(mFrameMap->GetRtv()), true, GetRvaluePtr(mDepthStencilMap->GetDsv()));

	gGraphicsCommandList->SetPipelineState(mPBRStaticMeshPSO->Get());
	ExecuteIndirect(GetIndirectCommandBuffer(OPAQUE_STATIC));

	gGraphicsCommandList->SetPipelineState(mPBRSkinnedMeshPSO->Get());
	ExecuteIndirect(GetIndirectCommandBuffer(OPAQUE_SKINNED));

	DrawSkyBox(gScene->GetSkyBox()->GetMeshCBAddress());
}

void Carol::FramePass::DrawSkyBox(D3D12_GPU_VIRTUAL_ADDRESS skyBoxMeshCBAddr)
{
	gGraphicsCommandList->SetPipelineState(mSkyBoxMeshPSO->Get());
	gGraphicsCommandList->SetGraphicsRootConstantBufferView(MESH_CB, skyBoxMeshCBAddr);
	static_cast<ID3D12GraphicsCommandList6*>(gGraphicsCommandList.Get())->DispatchMesh(1, 1, 1);
}

void Carol::FramePass::DrawTransparent()
{
	if (!gScene->IsAnyTransparentMeshes())
	{
		return;
	}

	constexpr uint32_t initOffsetValue = UINT32_MAX;
	constexpr uint32_t initCounterValue = 0;

	mOitppllBuffer->Transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	mStartOffsetBuffer->Transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	gGraphicsCommandList->ClearUnorderedAccessViewUint(mStartOffsetBuffer->GetGpuUav(), mStartOffsetBuffer->GetCpuUav(), mStartOffsetBuffer->Get(), &initOffsetValue, 0, nullptr);
	gGraphicsCommandList->ClearUnorderedAccessViewUint(mCounterBuffer->GetGpuUav(), mCounterBuffer->GetCpuUav(), mCounterBuffer->Get(), &initCounterValue, 0, nullptr);
	gGraphicsCommandList->OMSetRenderTargets(0, nullptr, true, nullptr);

	gGraphicsCommandList->SetPipelineState(mPBRStaticOitppllMeshPSO->Get());
	ExecuteIndirect( GetIndirectCommandBuffer(TRANSPARENT_STATIC));

	gGraphicsCommandList->SetPipelineState(mPBRSkinnedOitppllMeshPSO->Get());
	ExecuteIndirect(GetIndirectCommandBuffer(TRANSPARENT_SKINNED));

	mOitppllBuffer->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mStartOffsetBuffer->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	gGraphicsCommandList->OMSetRenderTargets(1, GetRvaluePtr(mFrameMap->GetRtv()), true, nullptr);
	gGraphicsCommandList->SetPipelineState(mDrawOitppllMeshPSO->Get());
	static_cast<ID3D12GraphicsCommandList6*>(gGraphicsCommandList.Get())->DispatchMesh(1, 1, 1);
}