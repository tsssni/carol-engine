#include <render_pass/shadow.h>
#include <global.h>
#include <render_pass/display.h>
#include <render_pass/scene.h>
#include <dx12.h>
#include <scene/assimp.h>
#include <scene/camera.h>
#include <utils/common.h>
#include <DirectXColors.h>
#include <memory>
#include <string_view>

namespace Carol {
	using std::vector;
	using std::wstring;
	using std::wstring_view;
	using std::unique_ptr;
	using std::make_unique;
	using namespace DirectX;
}

Carol::ShadowPass::ShadowPass(
	Light light,
	uint32_t width,
	uint32_t height,
	uint32_t depthBias,
	float depthBiasClamp,
	float slopeScaledDepthBias,
	DXGI_FORMAT shadowFormat,
	DXGI_FORMAT hiZFormat)
	:mLight(make_unique<Light>(light)),
	mDepthBias(depthBias),
	mDepthBiasClamp(depthBiasClamp),
	mSlopeScaledDepthBias(slopeScaledDepthBias),
	mCullIdx(OPAQUE_MESH_TYPE_COUNT),
	mHiZIdx(HIZ_IDX_COUNT),
	mHiZMipLevels(std::max(ceilf(log2f(width)), ceilf(log2f(height)))),
	mCulledCommandBuffer(gNumFrame),
	mShadowFormat(shadowFormat),
	mHiZFormat(hiZFormat)
{
	InitLightView();
	InitCamera();
	InitShaders();
	InitPSOs();
	OnResize(width, height);
}

void Carol::ShadowPass::Draw()
{
	Clear();
	gCommandList->RSSetViewports(1, &mViewport);
	gCommandList->RSSetScissorRects(1, &mScissorRect);

	GenerateHiZ();
	CullMeshes(true);
	DrawShadow(true);

	GenerateHiZ();
	CullMeshes(false);
	DrawShadow(false);
}

void Carol::ShadowPass::Update()
{
	XMMATRIX view = mCamera->GetView();
	XMMATRIX proj = mCamera->GetProj();
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	static XMMATRIX tex(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f
	);
	
	XMStoreFloat4x4(&mLight->View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mLight->Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mLight->ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mLight->ViewProjTex, XMMatrixTranspose(DirectX::XMMatrixMultiply(viewProj, tex)));

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(OPAQUE_MESH_START + i);
		TestCommandBufferSize(mCulledCommandBuffer[gCurrFrame][type], gScene->GetMeshesCount(type));

		mCullIdx[i][CULL_CULLED_COMMAND_BUFFER_IDX] = mCulledCommandBuffer[gCurrFrame][i]->GetGpuUavIdx();
		mCullIdx[i][CULL_MESH_COUNT] = gScene->GetMeshesCount(type);
		mCullIdx[i][CULL_MESH_OFFSET] = gScene->GetMeshCBStartOffet(type);
		mCullIdx[i][CULL_HIZ_IDX] = mHiZMap->GetGpuSrvIdx();
		mCullIdx[i][CULL_LIGHT_IDX] = 0;
	}

	mHiZIdx[HIZ_DEPTH_IDX] = mShadowMap->GetGpuSrvIdx();
	mHiZIdx[HIZ_R_IDX] = mHiZMap->GetGpuSrvIdx();
	mHiZIdx[HIZ_W_IDX] = mHiZMap->GetGpuUavIdx();
}

void Carol::ShadowPass::ReleaseIntermediateBuffers()
{
}

uint32_t Carol::ShadowPass::GetShadowSrvIdx()
{
	return mShadowMap->GetGpuSrvIdx();
}

const Carol::Light& Carol::ShadowPass::GetLight()
{
	return *mLight;
}

void Carol::ShadowPass::InitBuffers()
{
	D3D12_CLEAR_VALUE optClearValue;
	optClearValue.Format = GetDsvFormat(mShadowFormat);
	optClearValue.DepthStencil.Depth = 1.0f;
	optClearValue.DepthStencil.Stencil = 0;

	mShadowMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mShadowFormat,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		&optClearValue);

	mHiZMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mHiZFormat,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		nullptr,
		mHiZMipLevels);

	for (int i = 0; i < gNumFrame; ++i)
	{
		mCulledCommandBuffer[i].resize(OPAQUE_MESH_TYPE_COUNT);

		for (int j = 0; j < OPAQUE_MESH_TYPE_COUNT; ++j)
		{
			ResizeCommandBuffer(mCulledCommandBuffer[i][j], 1024, sizeof(IndirectCommand));
		}
	}

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		mCullIdx[i].resize(CULL_IDX_COUNT);
	}
}

void Carol::ShadowPass::InitLightView()
{
	mViewport.TopLeftX = 0.0f;
	mViewport.TopLeftY = 0.0f;
	mViewport.Width = mWidth;
	mViewport.Height = mHeight;
	mViewport.MinDepth = 0.0f;
	mViewport.MaxDepth = 1.0f;
	mScissorRect = { 0,0,(int)mWidth,(int)mHeight };
}

void Carol::ShadowPass::InitCamera()
{
	mLight->Position = XMFLOAT3(-mLight->Direction.x * 140.f, -mLight->Direction.y * 140.f, -mLight->Direction.z * 140.f);
	mCamera = make_unique<OrthographicCamera>(mLight->Direction, DirectX::XMFLOAT3{0.0f,0.0f,0.0f}, 70.0f);
}

void Carol::ShadowPass::Clear()
{
	gScene->ClearCullMark();

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[gCurrFrame][i]->Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST)));
		mCulledCommandBuffer[gCurrFrame][i]->ResetCounter();
		gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[gCurrFrame][i]->Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)));
	}
}

void Carol::ShadowPass::CullMeshes(bool hist)
{
	gCommandList->SetPipelineState(gPSOs[L"ShadowInstanceCull"]->Get());
	
	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		if (mCullIdx[i][CULL_MESH_COUNT] == 0)
		{
			continue;
		}
		
		mCullIdx[i][CULL_HIST] = hist;
		uint32_t count = ceilf(mCullIdx[i][CULL_MESH_COUNT] / 32.f);

		gCommandList->SetComputeRoot32BitConstants(PASS_CONSTANTS, CULL_IDX_COUNT, mCullIdx[i].data(), 0);
		gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[gCurrFrame][i]->Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)));
		gCommandList->Dispatch(count, 1, 1);
		gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::UAV(mCulledCommandBuffer[gCurrFrame][i]->Get())));
	}
}

void Carol::ShadowPass::DrawShadow(bool hist)
{
	gCommandList->ClearDepthStencilView(mShadowMap->GetDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	gCommandList->OMSetRenderTargets(0, nullptr, true, GetRvaluePtr(mShadowMap->GetDsv()));

	ID3D12PipelineState* pso[] = {gPSOs[L"ShadowStatic"]->Get(), gPSOs[L"ShadowSkinned"]->Get()};

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		if (mCullIdx[i][CULL_MESH_COUNT] == 0)
		{
			continue;
		}

		mCullIdx[i][CULL_HIST] = hist;

		gCommandList->SetPipelineState(pso[i]);
		gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[gCurrFrame][i]->Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)));
		gCommandList->SetGraphicsRoot32BitConstants(PASS_CONSTANTS, CULL_IDX_COUNT, mCullIdx[i].data(), 0);
		gScene->ExecuteIndirect(mCulledCommandBuffer[gCurrFrame][i].get());
	}
}

void Carol::ShadowPass::GenerateHiZ()
{
	gCommandList->SetPipelineState(gPSOs[L"HiZGenerate"]->Get());

	for (int i = 0; i < mHiZMipLevels - 1; i += 5)
	{
		mHiZIdx[HIZ_SRC_MIP] = i;
		mHiZIdx[HIZ_NUM_MIP_LEVEL] = i + 5 >= mHiZMipLevels ? mHiZMipLevels - i - 1 : 5;
		gCommandList->SetComputeRoot32BitConstants(PASS_CONSTANTS, HIZ_IDX_COUNT, mHiZIdx.data(), 0);
		
		uint32_t width = ceilf((mWidth >> i) / 32.f);
		uint32_t height = ceilf((mHeight >> i) / 32.f);
		gCommandList->Dispatch(width, height, 1);
		gCommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::UAV(mHiZMap->Get())));
	}
}

void Carol::ShadowPass::TestCommandBufferSize(unique_ptr<StructuredBuffer>& buffer, uint32_t numElements)
{
	if (buffer->GetNumElements() < numElements)
	{
		ResizeCommandBuffer(buffer, numElements, buffer->GetElementSize());
	}
}

void Carol::ShadowPass::ResizeCommandBuffer(unique_ptr<StructuredBuffer>& buffer, uint32_t numElements, uint32_t elementSize)
{
	buffer = make_unique<StructuredBuffer>(
		numElements,
		elementSize,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}

void Carol::ShadowPass::InitShaders()
{
	vector<wstring_view> nullDefines{};

	vector<wstring_view> shadowDefines =
	{
		L"SHADOW=1"
	};

	vector<wstring_view> skinnedShadowDefines =
	{
		L"SKINNED=1",
		L"SHADOW=1"
	};

	vector<wstring_view> shadowCullDefines =
	{
		L"SHADOW=1",
		L"OCCLUSION=1",
		L"WRITE=1"
	};

	gShaders[L"ShadowCullCS"] = make_unique<Shader>(L"shader\\cull_cs.hlsl", shadowCullDefines, L"main", L"cs_6_6");
	gShaders[L"ShadowAS"] = make_unique<Shader>(L"shader\\cull_as.hlsl", shadowCullDefines, L"main", L"as_6_6");
	gShaders[L"ShadowStaticMS"] = make_unique<Shader>(L"shader\\depth_ms.hlsl", shadowDefines, L"main", L"ms_6_6");
	gShaders[L"ShadowSkinnedMS"] = make_unique<Shader>(L"shader\\depth_ms.hlsl", skinnedShadowDefines, L"main", L"ms_6_6");
}

void Carol::ShadowPass::InitPSOs()
{
	auto shadowStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	shadowStaticMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	shadowStaticMeshPSO->SetDepthTargetFormat(GetDsvFormat(mShadowFormat));
	shadowStaticMeshPSO->SetAS(gShaders[L"ShadowAS"].get());
	shadowStaticMeshPSO->SetMS(gShaders[L"ShadowStaticMS"].get());
	shadowStaticMeshPSO->Finalize();
	gPSOs[L"ShadowStatic"] = std::move(shadowStaticMeshPSO);

	auto shadowSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
	shadowSkinnedMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
	shadowSkinnedMeshPSO->SetDepthTargetFormat(GetDsvFormat(mShadowFormat));
	shadowSkinnedMeshPSO->SetAS(gShaders[L"ShadowAS"].get());
	shadowSkinnedMeshPSO->SetMS(gShaders[L"ShadowSkinnedMS"].get());
	shadowSkinnedMeshPSO->Finalize();
	gPSOs[L"ShadowSkinned"] = std::move(shadowSkinnedMeshPSO);

	auto shadowInstanceCullComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	shadowInstanceCullComputePSO->SetCS(gShaders[L"ShadowCullCS"].get());
	shadowInstanceCullComputePSO->Finalize();
	gPSOs[L"ShadowInstanceCull"] = std::move(shadowInstanceCullComputePSO);
}