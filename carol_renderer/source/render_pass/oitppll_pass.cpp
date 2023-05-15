#include <render_pass/oitppll_pass.h>
#include <dx12/heap.h>
#include <dx12/resource.h>
#include <dx12/pipeline_state.h>
#include <dx12/shader.h>
#include <global.h>

namespace Carol
{
	using std::make_unique;
}

Carol::OitppllPass::OitppllPass()
	:mIndirectCommandBuffer(TRANSPARENT_MESH_TYPE_COUNT)
{
	InitPSOs();
}

void Carol::OitppllPass::Draw()
{
	constexpr uint32_t initOffsetValue = UINT32_MAX;
	constexpr uint32_t initCounterValue = 0;

	mPpllBuffer->Transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	mStartOffsetBuffer->Transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	gGraphicsCommandList->ClearUnorderedAccessViewUint(mStartOffsetBuffer->GetGpuUav(), mStartOffsetBuffer->GetCpuUav(), mStartOffsetBuffer->Get(), &initOffsetValue, 0, nullptr);
	gGraphicsCommandList->ClearUnorderedAccessViewUint(mCounterBuffer->GetGpuUav(), mCounterBuffer->GetCpuUav(), mCounterBuffer->Get(), &initCounterValue, 0, nullptr);
	gGraphicsCommandList->OMSetRenderTargets(0, nullptr, true, nullptr);

	gGraphicsCommandList->SetPipelineState(mBuildOitppllStaticMeshPSO->Get());
	ExecuteIndirect( mIndirectCommandBuffer[TRANSPARENT_STATIC - TRANSPARENT_MESH_START]);

	gGraphicsCommandList->SetPipelineState(mBuildOitppllSkinnedMeshPSO->Get());
	ExecuteIndirect( mIndirectCommandBuffer[TRANSPARENT_SKINNED - TRANSPARENT_MESH_START]);

	mPpllBuffer->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	mStartOffsetBuffer->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	uint32_t groupWidth = ceilf(mWidth * 1.f / 32.f);
	uint32_t groupHeight = ceilf(mHeight * 1.f / 32.f);

	gGraphicsCommandList->SetPipelineState(mOitppllComputePSO->Get());
	gGraphicsCommandList->Dispatch(groupWidth, groupHeight, 1);
}

void Carol::OitppllPass::SetIndirectCommandBuffer(MeshType type, const StructuredBuffer* indirectCommandBuffer)
{
    mIndirectCommandBuffer[type - TRANSPARENT_MESH_START] = indirectCommandBuffer;
}

uint32_t Carol::OitppllPass::GetPpllUavIdx() const
{
	return mPpllBuffer->GetGpuUavIdx();
}

uint32_t Carol::OitppllPass::GetStartOffsetUavIdx() const
{
	return mStartOffsetBuffer->GetGpuUavIdx();
}

uint32_t Carol::OitppllPass::GetCounterUavIdx() const
{
	return mCounterBuffer->GetGpuUavIdx();
}

uint32_t Carol::OitppllPass::GetPpllSrvIdx() const
{
	return mPpllBuffer->GetGpuSrvIdx();
}

uint32_t Carol::OitppllPass::GetStartOffsetSrvIdx() const
{
	return mStartOffsetBuffer->GetGpuSrvIdx();
}

void Carol::OitppllPass::InitPSOs()
{
	auto cullDisabledState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	cullDisabledState.CullMode = D3D12_CULL_MODE_NONE;

	auto depthDisabledState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	depthDisabledState.DepthEnable = false;
	depthDisabledState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

	auto alphaBlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	alphaBlendState.RenderTarget[0].BlendEnable = true;
	alphaBlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	alphaBlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	alphaBlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	alphaBlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	alphaBlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
	alphaBlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	alphaBlendState.RenderTarget[0].LogicOpEnable = false;
	alphaBlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	alphaBlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	mBuildOitppllStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mBuildOitppllStaticMeshPSO->SetRasterizerState(&cullDisabledState);
	mBuildOitppllStaticMeshPSO->SetDepthStencilState(&depthDisabledState);
	mBuildOitppllStaticMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
	mBuildOitppllStaticMeshPSO->SetAS(gShaderManager->LoadShader("shader/dxil/cull_as.dxil"));
	mBuildOitppllStaticMeshPSO->SetMS(gShaderManager->LoadShader("shader/dxil/static_mesh_ms.dxil"));
	mBuildOitppllStaticMeshPSO->SetPS(gShaderManager->LoadShader("shader/dxil/oitppll_build_ps.dxil"));
	mBuildOitppllStaticMeshPSO->Finalize();

	mBuildOitppllSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mBuildOitppllSkinnedMeshPSO->SetRasterizerState(&cullDisabledState);
	mBuildOitppllSkinnedMeshPSO->SetDepthStencilState(&depthDisabledState);
	mBuildOitppllSkinnedMeshPSO->SetDepthTargetFormat(DXGI_FORMAT_UNKNOWN);
	mBuildOitppllSkinnedMeshPSO->SetAS(gShaderManager->LoadShader("shader/dxil/cull_as.dxil"));
	mBuildOitppllSkinnedMeshPSO->SetMS(gShaderManager->LoadShader("shader/dxil/skinned_mesh_ms.dxil"));
	mBuildOitppllSkinnedMeshPSO->SetPS(gShaderManager->LoadShader("shader/dxil/oitppll_build_ps.dxil"));
	mBuildOitppllSkinnedMeshPSO->Finalize();

	mOitppllComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mOitppllComputePSO->SetCS(gShaderManager->LoadShader("shader/dxil/oitppll_cs.dxil"));
	mOitppllComputePSO->Finalize();
}

void Carol::OitppllPass::InitBuffers()
{
	mPpllBuffer = make_unique<StructuredBuffer>(
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
}

