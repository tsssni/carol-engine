#include <render_pass/frame_pass.h>
#include <render_pass/cull_pass.h>
#include <scene.h>
#include <dx12.h>
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
	ID3D12Device* device,
	Heap* defaultBuffersHeap,
	Heap* uploadBuffersHeap,
	DescriptorManager* descriptorManager,
	Scene* scene,
	DXGI_FORMAT frameFormat,
	DXGI_FORMAT depthStencilFormat,
	DXGI_FORMAT hiZFormat)
	:mScene(scene),
	mFrameFormat(frameFormat),
	mDepthStencilFormat(depthStencilFormat)
{
	InitPSOs(device);

	mCullPass = make_unique<CullPass>(
		device,
		defaultBuffersHeap,
		uploadBuffersHeap,
		descriptorManager,
		scene,
		0,
		0.f,
		0.f,
		depthStencilFormat);
}

void Carol::FramePass::Draw(ID3D12GraphicsCommandList* cmdList)
{
	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	mFrameMap->Transition(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	cmdList->ClearRenderTargetView(mFrameMap->GetRtv(), Colors::Gray, 0, nullptr);
	cmdList->ClearDepthStencilView(mDepthStencilMap->GetDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);

	DrawOpaque(cmdList);
	DrawTransparent(cmdList);
}

void Carol::FramePass::Update(XMMATRIX viewProj, XMMATRIX histViewProj, XMVECTOR eyePos, uint64_t cpuFenceValue, uint64_t completedFenceValue)
{
	mCullPass->Update(viewProj, histViewProj, eyePos, cpuFenceValue, completedFenceValue);
}

void Carol::FramePass::Cull(ID3D12GraphicsCommandList* cmdList)
{
	mCullPass->Draw(cmdList);
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

void Carol::FramePass::InitPSOs(ID3D12Device* device)
{
	mBlinnPhongStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mBlinnPhongStaticMeshPSO->SetRootSignature(sRootSignature.get());
	mBlinnPhongStaticMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
	mBlinnPhongStaticMeshPSO->SetAS(&gCullAS);
	mBlinnPhongStaticMeshPSO->SetMS(&gMeshStaticMS);
	mBlinnPhongStaticMeshPSO->SetPS(&gBlinnPhongPS);
	mBlinnPhongStaticMeshPSO->Finalize(device);

	mBlinnPhongSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mBlinnPhongSkinnedMeshPSO->SetRootSignature(sRootSignature.get());
	mBlinnPhongSkinnedMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
	mBlinnPhongSkinnedMeshPSO->SetAS(&gCullAS);
	mBlinnPhongSkinnedMeshPSO->SetMS(&gMeshSkinnedMS);
	mBlinnPhongSkinnedMeshPSO->SetPS(&gBlinnPhongPS);
	mBlinnPhongSkinnedMeshPSO->Finalize(device);

	mPBRStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mPBRStaticMeshPSO->SetRootSignature(sRootSignature.get());
	mPBRStaticMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
	mPBRStaticMeshPSO->SetAS(&gCullAS);
	mPBRStaticMeshPSO->SetMS(&gMeshStaticMS);
	mPBRStaticMeshPSO->SetPS(&gPBRPS);
	mPBRStaticMeshPSO->Finalize(device);

	mPBRSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mPBRSkinnedMeshPSO->SetRootSignature(sRootSignature.get());
	mPBRSkinnedMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
	mPBRSkinnedMeshPSO->SetAS(&gCullAS);
	mPBRSkinnedMeshPSO->SetMS(&gMeshSkinnedMS);
	mPBRSkinnedMeshPSO->SetPS(&gPBRPS);
	mPBRSkinnedMeshPSO->Finalize(device);

	mBlinnPhongStaticOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mBlinnPhongStaticOitppllMeshPSO->SetRootSignature(sRootSignature.get());
	mBlinnPhongStaticOitppllMeshPSO->SetRasterizerState(gCullDisabledState);
	mBlinnPhongStaticOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState);
	mBlinnPhongStaticOitppllMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
	mBlinnPhongStaticOitppllMeshPSO->SetAS(&gCullAS);
	mBlinnPhongStaticOitppllMeshPSO->SetMS(&gMeshStaticMS);
	mBlinnPhongStaticOitppllMeshPSO->SetPS(&gBlinnPhongOitppllPS);
	mBlinnPhongStaticOitppllMeshPSO->Finalize(device);

	mBlinnPhongSkinnedOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mBlinnPhongSkinnedOitppllMeshPSO->SetRootSignature(sRootSignature.get());
	mBlinnPhongSkinnedOitppllMeshPSO->SetRasterizerState(gCullDisabledState);
	mBlinnPhongSkinnedOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState);
	mBlinnPhongSkinnedOitppllMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
	mBlinnPhongSkinnedOitppllMeshPSO->SetAS(&gCullAS);
	mBlinnPhongSkinnedOitppllMeshPSO->SetMS(&gMeshSkinnedMS);
	mBlinnPhongSkinnedOitppllMeshPSO->SetPS(&gBlinnPhongOitppllPS);
	mBlinnPhongSkinnedOitppllMeshPSO->Finalize(device);

	mPBRStaticOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mPBRStaticOitppllMeshPSO->SetRootSignature(sRootSignature.get());
	mPBRStaticOitppllMeshPSO->SetRasterizerState(gCullDisabledState);
	mPBRStaticOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState);
	mPBRStaticOitppllMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
	mPBRStaticOitppllMeshPSO->SetAS(&gCullAS);
	mPBRStaticOitppllMeshPSO->SetMS(&gMeshStaticMS);
	mPBRStaticOitppllMeshPSO->SetPS(&gPBROitppllPS);
	mPBRStaticOitppllMeshPSO->Finalize(device);

	mPBRSkinnedOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mPBRSkinnedOitppllMeshPSO->SetRootSignature(sRootSignature.get());
	mPBRSkinnedOitppllMeshPSO->SetRasterizerState(gCullDisabledState);
	mPBRSkinnedOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState);
	mPBRSkinnedOitppllMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
	mPBRSkinnedOitppllMeshPSO->SetAS(&gCullAS);
	mPBRSkinnedOitppllMeshPSO->SetMS(&gMeshSkinnedMS);
	mPBRSkinnedOitppllMeshPSO->SetPS(&gPBROitppllPS);
	mPBRSkinnedOitppllMeshPSO->Finalize(device);

	mDrawOitppllMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mDrawOitppllMeshPSO->SetRootSignature(sRootSignature.get());
	mDrawOitppllMeshPSO->SetDepthStencilState(gDepthDisabledState);
	mDrawOitppllMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
	mDrawOitppllMeshPSO->SetBlendState(gAlphaBlendState);
	mDrawOitppllMeshPSO->SetRenderTargetFormat(mFrameFormat);
	mDrawOitppllMeshPSO->SetMS(&gScreenMS);
	mDrawOitppllMeshPSO->SetPS(&gDrawOitppllPS);
	mDrawOitppllMeshPSO->Finalize(device);
	
	mSkyBoxMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mSkyBoxMeshPSO->SetRootSignature(sRootSignature.get());
	mSkyBoxMeshPSO->SetDepthStencilState(gDepthLessEqualState);
	mSkyBoxMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
	mSkyBoxMeshPSO->SetMS(&gSkyBoxMS);
	mSkyBoxMeshPSO->SetPS(&gSkyBoxPS);
	mSkyBoxMeshPSO->Finalize(device);
}

void Carol::FramePass::InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)
{
	mOitppllBuffer = make_unique<StructuredBuffer>(
		mWidth * mHeight * 16,
		sizeof(OitppllNode),
		device,
		heap,
		descriptorManager,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mStartOffsetBuffer = make_unique<RawBuffer>(
		mWidth * mHeight * sizeof(uint32_t),
		device,
		heap,
		descriptorManager,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mCounterBuffer = make_unique<RawBuffer>(
		sizeof(uint32_t),
		device,
		heap,
		descriptorManager,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mCullPass->OnResize(mWidth, mHeight, device, heap, descriptorManager);
}

void Carol::FramePass::DrawOpaque(ID3D12GraphicsCommandList* cmdList)
{
	cmdList->OMSetRenderTargets(1, GetRvaluePtr(mFrameMap->GetRtv()), true, GetRvaluePtr(mDepthStencilMap->GetDsv()));

	cmdList->SetPipelineState(mPBRStaticMeshPSO->Get());
	ExecuteIndirect(cmdList, GetIndirectCommandBuffer(OPAQUE_STATIC));

	cmdList->SetPipelineState(mPBRSkinnedMeshPSO->Get());
	ExecuteIndirect(cmdList, GetIndirectCommandBuffer(OPAQUE_SKINNED));

	DrawSkyBox(cmdList, mScene->GetSkyBox()->GetMeshCBAddress());
}

void Carol::FramePass::DrawSkyBox(ID3D12GraphicsCommandList* cmdList, D3D12_GPU_VIRTUAL_ADDRESS skyBoxMeshCBAddr)
{
	cmdList->SetPipelineState(mSkyBoxMeshPSO->Get());
	cmdList->SetGraphicsRootConstantBufferView(MESH_CB, skyBoxMeshCBAddr);
	static_cast<ID3D12GraphicsCommandList6*>(cmdList)->DispatchMesh(1, 1, 1);
}

void Carol::FramePass::DrawTransparent(ID3D12GraphicsCommandList* cmdList)
{
	if (!mScene->IsAnyTransparentMeshes())
	{
		return;
	}

	constexpr uint32_t initOffsetValue = UINT32_MAX;
	constexpr uint32_t initCounterValue = 0;

	mOitppllBuffer->Transition(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	mStartOffsetBuffer->Transition(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	cmdList->ClearUnorderedAccessViewUint(mStartOffsetBuffer->GetGpuUav(), mStartOffsetBuffer->GetCpuUav(), mStartOffsetBuffer->Get(), &initOffsetValue, 0, nullptr);
	cmdList->ClearUnorderedAccessViewUint(mCounterBuffer->GetGpuUav(), mCounterBuffer->GetCpuUav(), mCounterBuffer->Get(), &initCounterValue, 0, nullptr);
	cmdList->OMSetRenderTargets(0, nullptr, true, nullptr);

	cmdList->SetPipelineState(mPBRStaticOitppllMeshPSO->Get());
	ExecuteIndirect(cmdList, GetIndirectCommandBuffer(TRANSPARENT_STATIC));

	cmdList->SetPipelineState(mPBRSkinnedOitppllMeshPSO->Get());
	ExecuteIndirect(cmdList, GetIndirectCommandBuffer(TRANSPARENT_SKINNED));

	mOitppllBuffer->Transition(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mStartOffsetBuffer->Transition(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	cmdList->OMSetRenderTargets(1, GetRvaluePtr(mFrameMap->GetRtv()), true, nullptr);
	cmdList->SetPipelineState(mDrawOitppllMeshPSO->Get());
	static_cast<ID3D12GraphicsCommandList6*>(cmdList)->DispatchMesh(1, 1, 1);
}