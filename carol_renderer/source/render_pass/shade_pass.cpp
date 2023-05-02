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

Carol::ShadePass::ShadePass(
	DXGI_FORMAT frameFormat,
	DXGI_FORMAT depthStencilFormat,
	DXGI_FORMAT hiZFormat)
	:mFrameFormat(frameFormat),
	mDepthStencilFormat(depthStencilFormat)
{
	InitPSOs();
}

void Carol::ShadePass::Draw()
{
	uint32_t groupWidth = ceilf(mWidth * 1.f / 32.f);
	uint32_t groupHeight = ceilf(mHeight * 1.f / 32.f);

	gGraphicsCommandList->SetPipelineState(mShadeComputePSO->Get());
	gGraphicsCommandList->Dispatch(groupWidth, groupHeight, 1);

	mFrameMap->Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);
	gGraphicsCommandList->OMSetRenderTargets(1, GetRvaluePtr(mFrameMap->GetRtv()), true, GetRvaluePtr(mDepthStencilMap->GetDsv()));
	gGraphicsCommandList->SetPipelineState(mSkyBoxMeshPSO->Get());
	gGraphicsCommandList->SetGraphicsRootConstantBufferView(MESH_CB, gScene->GetSkyBox()->GetMeshCBAddress());
	static_cast<ID3D12GraphicsCommandList6*>(gGraphicsCommandList.Get())->DispatchMesh(1, 1, 1);
	mFrameMap->Transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void Carol::ShadePass::SetFrameMap(ColorBuffer* frameMap)
{
	mFrameMap = frameMap;
}

void Carol::ShadePass::SetDepthStencilMap(ColorBuffer* depthStencilMap)
{
	mDepthStencilMap = depthStencilMap;
}

void Carol::ShadePass::InitPSOs()
{	
	mShadeComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mShadeComputePSO->SetRootSignature(sRootSignature.get());
	mShadeComputePSO->SetCS(gShadeCS.get());
	mShadeComputePSO->Finalize();

	mSkyBoxMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mSkyBoxMeshPSO->SetRootSignature(sRootSignature.get());
	mSkyBoxMeshPSO->SetDepthStencilState(gDepthLessEqualState.get());
	mSkyBoxMeshPSO->SetRenderTargetFormat(mFrameFormat, GetDsvFormat(mDepthStencilFormat));
	mSkyBoxMeshPSO->SetMS(gSkyBoxMS.get());
	mSkyBoxMeshPSO->SetPS(gSkyBoxPS.get());
	mSkyBoxMeshPSO->Finalize();
}

void Carol::ShadePass::InitBuffers()
{
}

void Carol::ShadePass::DrawSkyBox(D3D12_GPU_VIRTUAL_ADDRESS skyBoxMeshCBAddr)
{
}