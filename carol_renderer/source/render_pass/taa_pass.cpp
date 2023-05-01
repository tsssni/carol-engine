#include <render_pass/taa_pass.h>
#include <scene/scene.h>
#include <dx12.h>
#include <utils/common.h>
#include <DirectXColors.h>
#include <cmath>
#include <string_view>

#define BORDER_RADIUS 1

namespace Carol {
	using std::vector;
	using std::wstring;
	using std::wstring_view;
	using std::make_unique;
	using Microsoft::WRL::ComPtr;
}

Carol::TaaPass::TaaPass(
	ID3D12Device* device,
	DXGI_FORMAT frameMapFormat,
	DXGI_FORMAT velocityMapFormat,
	DXGI_FORMAT velocityDsvFormat)
	:mFrameMapFormat(frameMapFormat),
	mVelocityDsvFormat(velocityDsvFormat),
	mVelocityMapFormat(velocityMapFormat),
	mIndirectCommandBuffer(MESH_TYPE_COUNT)
{
	InitHalton();
	InitPSOs(device);
}

void Carol::TaaPass::InitPSOs(ID3D12Device* device)
{
	mVelocityStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mVelocityStaticMeshPSO->SetRootSignature(sRootSignature.get());
	mVelocityStaticMeshPSO->SetRenderTargetFormat(mVelocityMapFormat, mVelocityDsvFormat);
	mVelocityStaticMeshPSO->SetAS(&gCullAS);
	mVelocityStaticMeshPSO->SetMS(&gVelocityStaticMS);
	mVelocityStaticMeshPSO->SetPS(&gVelocityPS);
	mVelocityStaticMeshPSO->Finalize(device);

	mVelocitySkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	mVelocitySkinnedMeshPSO->SetRootSignature(sRootSignature.get());
	mVelocitySkinnedMeshPSO->SetRenderTargetFormat(mVelocityMapFormat, mVelocityDsvFormat);
	mVelocitySkinnedMeshPSO->SetAS(&gCullAS);
	mVelocitySkinnedMeshPSO->SetMS(&gVelocitySkinnedMS);
	mVelocitySkinnedMeshPSO->SetPS(&gVelocityPS);
	mVelocitySkinnedMeshPSO->Finalize(device);

	mTaaComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mTaaComputePSO->SetRootSignature(sRootSignature.get());
	mTaaComputePSO->SetCS(&gTaaCS);
	mTaaComputePSO->Finalize(device);
}

void Carol::TaaPass::Draw(ID3D12GraphicsCommandList* cmdList)
{
	DrawVelocityMap(cmdList);
	DrawOutput(cmdList);
}

void Carol::TaaPass::SetFrameMap(ColorBuffer* frameMap)
{
	mFrameMap = frameMap;
}

void Carol::TaaPass::SetDepthStencilMap(ColorBuffer* depthStencilMap)
{
	mDepthStencilMap = depthStencilMap;
}

void Carol::TaaPass::SetIndirectCommandBuffer(MeshType type, const StructuredBuffer* indirectCommandBuffer)
{
	mIndirectCommandBuffer[type] = indirectCommandBuffer;
}

void Carol::TaaPass::DrawVelocityMap(ID3D12GraphicsCommandList* cmdList)
{
	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	mVelocityMap->Transition(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
	cmdList->ClearRenderTargetView(mVelocityMap->GetRtv(), DirectX::Colors::Black, 0, nullptr);
	cmdList->ClearDepthStencilView(mDepthStencilMap->GetDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);
	cmdList->OMSetRenderTargets(1, GetRvaluePtr(mVelocityMap->GetRtv()), true, GetRvaluePtr(mDepthStencilMap->GetDsv()));

	cmdList->SetPipelineState(mVelocityStaticMeshPSO->Get());
	ExecuteIndirect(cmdList, mIndirectCommandBuffer[OPAQUE_STATIC]);
	ExecuteIndirect(cmdList, mIndirectCommandBuffer[TRANSPARENT_STATIC]);

	cmdList->SetPipelineState(mVelocitySkinnedMeshPSO->Get());
	ExecuteIndirect(cmdList, mIndirectCommandBuffer[OPAQUE_SKINNED]);
	ExecuteIndirect(cmdList, mIndirectCommandBuffer[TRANSPARENT_SKINNED]);

	mVelocityMap->Transition(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void Carol::TaaPass::DrawOutput(ID3D12GraphicsCommandList* cmdList)
{
	uint32_t groupWidth = ceilf(mWidth * 1.f / (32 - 2 * BORDER_RADIUS)); 
    uint32_t groupHeight = ceilf(mHeight * 1.f / (32 - 2 * BORDER_RADIUS));

	mFrameMap->Transition(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	cmdList->SetPipelineState(mTaaComputePSO->Get());
	cmdList->Dispatch(groupWidth, groupHeight, 1);
}

void Carol::TaaPass::GetHalton(float& proj0, float& proj1)const
{
	static int i = 0;

	proj0 = (2 * mHalton[i].x - 1) / mWidth;
	proj1 = (2 * mHalton[i].y - 1) / mHeight;

	i = (i + 1) % 8;
}

void Carol::TaaPass::SetHistViewProj(DirectX::XMMATRIX& histViewProj)
{
	mHistViewProj = histViewProj;
}

DirectX::XMMATRIX Carol::TaaPass::GetHistViewProj()const
{
	return mHistViewProj;
}

uint32_t Carol::TaaPass::GetVeloctiySrvIdx()const
{
	return mVelocityMap->GetGpuSrvIdx();
}

uint32_t Carol::TaaPass::GetHistFrameUavIdx()const
{
	return mHistMap->GetGpuUavIdx();
}

void Carol::TaaPass::InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)
{
	D3D12_CLEAR_VALUE optClearValue = CD3DX12_CLEAR_VALUE(mVelocityMapFormat, DirectX::Colors::Black);
	mVelocityMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mVelocityMapFormat,
		device,
		heap,
		descriptorManager,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		&optClearValue);

	mHistMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mFrameMapFormat,
		device,
		heap,
		descriptorManager,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}

void Carol::TaaPass::InitHalton()
{
	for (int i = 0; i < 8; ++i)
	{
		mHalton[i].x = RadicalInversion(2, i + 1);
		mHalton[i].y = RadicalInversion(3, i + 1);
	}
}

float Carol::TaaPass::RadicalInversion(int base, int num)
{
	int temp[4];
	int i = 0;

	while (num != 0)
	{
		temp[i++] = num % base;
		num /= base;
	}

	double convertionResult = 0;
	for (int j = 0; j < i; ++j)
	{
		convertionResult += temp[j] * pow(base, -j - 1);
	}

	return convertionResult;
}
