#include <render_pass/shadow_pass.h>
#include <global.h>
#include <render_pass/display_pass.h>
#include <scene/scene.h>
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
	using std::span;
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
	InitLightViewport();
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
	CullInstances(true);
	DrawShadow(true);

	GenerateHiZ();
	CullInstances(false);
	DrawShadow(false);
}

void Carol::ShadowPass::Update(uint32_t lightIdx)
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
		mCullIdx[i][CULL_LIGHT_IDX] = lightIdx;
	}

	mHiZIdx[HIZ_DEPTH_IDX] = mShadowMap->GetGpuSrvIdx();
	mHiZIdx[HIZ_R_IDX] = mHiZMap->GetGpuSrvIdx();
	mHiZIdx[HIZ_W_IDX] = mHiZMap->GetGpuUavIdx();
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

void Carol::ShadowPass::InitLightViewport()
{
	mViewport.TopLeftX = 0.0f;
	mViewport.TopLeftY = 0.0f;
	mViewport.Width = mWidth;
	mViewport.Height = mHeight;
	mViewport.MinDepth = 0.0f;
	mViewport.MaxDepth = 1.0f;
	mScissorRect = { 0,0,(int)mWidth,(int)mHeight };
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

void Carol::ShadowPass::CullInstances(bool hist)
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

void Carol::ShadowPass::Update()
{
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
	
	if (gShaders.count(L"ShadowCullCS") == 0)
	{
		gShaders[L"ShadowCullCS"] = make_unique<Shader>(L"shader\\cull_cs.hlsl", shadowCullDefines, L"main", L"cs_6_6");
	}

	if (gShaders.count(L"ShadowAS") == 0)
	{
		gShaders[L"ShadowAS"] = make_unique<Shader>(L"shader\\cull_as.hlsl", shadowCullDefines, L"main", L"as_6_6");
	}
	
	if (gShaders.count(L"ShadowStaticMS") == 0)
	{
		gShaders[L"ShadowStaticMS"] = make_unique<Shader>(L"shader\\depth_ms.hlsl", shadowDefines, L"main", L"ms_6_6");
	}

	if (gShaders.count(L"ShadowSkinnedMS") == 0)
	{
		gShaders[L"ShadowSkinnedMS"] = make_unique<Shader>(L"shader\\depth_ms.hlsl", skinnedShadowDefines, L"main", L"ms_6_6");
	}
}

void Carol::ShadowPass::InitPSOs()
{
	if (gPSOs.count(L"ShadowStatic") == 0)
	{
		auto shadowStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
		shadowStaticMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
		shadowStaticMeshPSO->SetDepthTargetFormat(GetDsvFormat(mShadowFormat));
		shadowStaticMeshPSO->SetAS(gShaders[L"ShadowAS"].get());
		shadowStaticMeshPSO->SetMS(gShaders[L"ShadowStaticMS"].get());
		shadowStaticMeshPSO->Finalize();
	
		gPSOs[L"ShadowStatic"] = std::move(shadowStaticMeshPSO);
	}

	if (gPSOs.count(L"ShadowSkinned") == 0)
	{
		auto shadowSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
		shadowSkinnedMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
		shadowSkinnedMeshPSO->SetDepthTargetFormat(GetDsvFormat(mShadowFormat));
		shadowSkinnedMeshPSO->SetAS(gShaders[L"ShadowAS"].get());
		shadowSkinnedMeshPSO->SetMS(gShaders[L"ShadowSkinnedMS"].get());
		shadowSkinnedMeshPSO->Finalize();
	
		gPSOs[L"ShadowSkinned"] = std::move(shadowSkinnedMeshPSO);
	}

	if (gPSOs.count(L"ShadowInstanceCull") == 0)
	{
		auto shadowInstanceCullComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
		shadowInstanceCullComputePSO->SetCS(gShaders[L"ShadowCullCS"].get());
		shadowInstanceCullComputePSO->Finalize();

		gPSOs[L"ShadowInstanceCull"] = std::move(shadowInstanceCullComputePSO);
	}
}

Carol::DirectLightShadowPass::DirectLightShadowPass(Light light, uint32_t width, uint32_t height, uint32_t depthBias, float depthBiasClamp, float slopeScaledDepthBias, DXGI_FORMAT shadowFormat, DXGI_FORMAT hiZFormat)
	:ShadowPass(light, width, height, depthBias, depthBiasClamp, slopeScaledDepthBias, shadowFormat, hiZFormat)
{
	InitCamera();
}

void Carol::DirectLightShadowPass::Update(uint32_t lightIdx, const PerspectiveCamera* camera, float zn, float zf)
{
	static float dx[4] = { -1.f,1.f,-1.f,1.f };
	static float dy[4] = { -1.f,-1.f,1.f,1.f };

	XMFLOAT4 pointNear;
	pointNear.z = zn;
	pointNear.y = zn * tanf(0.5f * camera->GetFovY());
	pointNear.x = zn * tanf(0.5f * camera->GetFovX());
	
	XMFLOAT4 pointFar;
	pointFar.z = zf;
	pointFar.y = zf * tanf(0.5f * camera->GetFovY());
	pointFar.x = zf * tanf(0.5f * camera->GetFovX());

	vector<XMFLOAT4> frustumSliceExtremaPoints;
	XMMATRIX perspView = camera->GetView();
	XMMATRIX invPerspView = XMMatrixInverse(GetRvaluePtr(XMMatrixDeterminant(perspView)), perspView);
	XMMATRIX orthoView = mCamera->GetView();
	XMMATRIX invOrthoView = XMMatrixInverse(GetRvaluePtr(XMMatrixDeterminant(orthoView)), orthoView);

	for (int i = 0; i < 4; ++i)
	{
		XMFLOAT4 point;
		
		point = { pointNear.x* dx[i], pointNear.y* dy[i], pointNear.z, 1.f };
		XMStoreFloat4(&point, XMVector4Transform(XMLoadFloat4(&point), XMMatrixMultiply(invPerspView, orthoView)));
		frustumSliceExtremaPoints.push_back(point);

		point = { pointFar.x* dx[i], pointFar.y* dy[i], pointFar.z, 1.f };
		XMStoreFloat4(&point, XMVector4Transform(XMLoadFloat4(&point), XMMatrixMultiply(invPerspView, orthoView)));
		frustumSliceExtremaPoints.push_back(point);
	}

	XMFLOAT4 boxMin = { D3D12_FLOAT32_MAX,D3D12_FLOAT32_MAX,D3D12_FLOAT32_MAX,1.f };
	XMFLOAT4 boxMax = { -D3D12_FLOAT32_MAX,-D3D12_FLOAT32_MAX,-D3D12_FLOAT32_MAX,1.f };

	for (auto& point : frustumSliceExtremaPoints)
	{
		boxMin.x = std::min(boxMin.x, point.x);
		boxMin.y = std::min(boxMin.y, point.y);
		boxMin.z = std::min(boxMin.z, point.z);

		boxMax.x = std::max(boxMax.x, point.x);
		boxMax.y = std::max(boxMax.y, point.y);
		boxMax.z = std::max(boxMax.z, point.z);
	}
	
	XMFLOAT4 center;
	XMStoreFloat4(&center, XMVector4Transform(0.5f * (XMLoadFloat4(&boxMin) + XMLoadFloat4(&boxMax)), invOrthoView));

	dynamic_cast<OrthographicCamera*>(mCamera.get())->SetLens(boxMax.x - boxMin.x, boxMax.y - boxMin.y, 1.f, boxMax.z + boxMax.z - boxMin.z);
	mCamera->LookAt(XMLoadFloat4(&center) - 0.5f * (boxMax.z - boxMin.z) * XMLoadFloat3(&mLight->Direction), XMLoadFloat4(&center), { 0.f,1.f,0.f,0.f });
	mCamera->UpdateViewMatrix();

	ShadowPass::Update(lightIdx);
}

void Carol::DirectLightShadowPass::InitCamera()
{
	mLight->Position = XMFLOAT3(-mLight->Direction.x * 140.f, -mLight->Direction.y * 140.f, -mLight->Direction.z * 140.f);
	mCamera = make_unique<OrthographicCamera>(70, 0, 280);
	mCamera->LookAt(mLight->Position, { 0.f,0.f,0.f }, { 0.f,1.f,0.f });
	mCamera->UpdateViewMatrix();
}

Carol::CascadedShadowPass::CascadedShadowPass(
	Light light,
	uint32_t splitLevel,
	uint32_t width,
	uint32_t height,
	uint32_t depthBias,
	float depthBiasClamp,
	float slopeScaledDepthBias,
	DXGI_FORMAT shadowFormat,
	DXGI_FORMAT hiZFormat)
	:mSplitLevel(splitLevel),
	mSplitZ(splitLevel + 1),
	mShadow(splitLevel)
{
	for (auto& shadow : mShadow)
	{
		shadow = make_unique<DirectLightShadowPass>(
			light,
			width,
			height,
			depthBias,
			depthBiasClamp,
			slopeScaledDepthBias,
			shadowFormat,
			hiZFormat);
	}
}

void Carol::CascadedShadowPass::Draw()
{
	for (auto& shadow : mShadow)
	{
		shadow->Draw();
	}
}

void Carol::CascadedShadowPass::Update(const PerspectiveCamera* camera, float logWeight, float bias)
{
	float nearZ = camera->GetNearZ();
	float farZ = camera->GetFarZ();

	for (int i = 0; i < mSplitLevel + 1; ++i)
	{
		mSplitZ[i] = logWeight * nearZ * pow(farZ / nearZ, 1.f * i / mSplitLevel) + (1 - logWeight) * (nearZ + (farZ - nearZ) * (1.f * i / mSplitLevel)) + bias;
	}

	for (int i = 0; i < mSplitLevel; ++i)
	{
		mShadow[i]->Update(i, camera, mSplitZ[i], mSplitZ[i + 1]);
	}
}

uint32_t Carol::CascadedShadowPass::GetSplitLevel()
{
	return mSplitLevel;
}

float Carol::CascadedShadowPass::GetSplitZ(uint32_t idx)
{
	return mSplitZ[idx];
}

uint32_t Carol::CascadedShadowPass::GetShadowSrvIdx(uint32_t idx)
{
	return mShadow[idx]->GetShadowSrvIdx();
}

const Carol::Light& Carol::CascadedShadowPass::GetLight(uint32_t idx)
{
	return mShadow[idx]->GetLight();
}

void Carol::CascadedShadowPass::Update()
{
}

void Carol::CascadedShadowPass::InitShaders()
{
}

void Carol::CascadedShadowPass::InitPSOs()
{
}

void Carol::CascadedShadowPass::InitBuffers()
{
}