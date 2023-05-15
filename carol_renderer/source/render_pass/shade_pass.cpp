#include <render_pass/shade_pass.h>
#include <dx12/resource.h>
#include <dx12/pipeline_state.h>
#include <dx12/root_signature.h>
#include <dx12/shader.h>
#include <scene/scene.h>
#include <global.h>
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

Carol::ShadePass::ShadePass()
{
	InitPSOs();
}

void Carol::ShadePass::Draw()
{
	uint32_t groupWidth = ceilf(mWidth * 1.f / 32.f);
	uint32_t groupHeight = ceilf(mHeight * 1.f / 32.f);

	gGraphicsCommandList->SetPipelineState(mShadeComputePSO->Get());
	gGraphicsCommandList->Dispatch(groupWidth, groupHeight, 1);
}

void Carol::ShadePass::InitPSOs()
{	
	mShadeComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mShadeComputePSO->SetCS(gShaderManager->LoadShader("shader/dxil/shade_cs.dxil"));
	mShadeComputePSO->Finalize();
}

void Carol::ShadePass::InitBuffers()
{
}