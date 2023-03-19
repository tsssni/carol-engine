#include <render_pass/shadow_pass.h>
#include <global.h>
#include <dx12.h>
#include <scene/scene.h>
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
	ID3D12Device* device,
	Heap* heap,
	DescriptorManager* descriptorManager,
	Scene* scene,
	Light light,
	uint32_t width,
	uint32_t height,
	uint32_t depthBias,
	float depthBiasClamp,
	float slopeScaledDepthBias,
	DXGI_FORMAT shadowFormat,
	DXGI_FORMAT hiZFormat)
	:mLight(make_unique<Light>(light)),
	mScene(scene),
	mDepthBias(depthBias),
	mDepthBiasClamp(depthBiasClamp),
	mSlopeScaledDepthBias(slopeScaledDepthBias),
	mCullIdx(OPAQUE_MESH_TYPE_COUNT),
	mHiZIdx(HIZ_IDX_COUNT),
	mHiZMipLevels(std::max(ceilf(log2f(width)), ceilf(log2f(height)))),
	mCulledCommandBuffer(OPAQUE_MESH_TYPE_COUNT),
	mShadowFormat(shadowFormat),
	mHiZFormat(hiZFormat)
{
	InitLight();
	InitShaders();
	InitPSOs(device);
	OnResize(
		width,
		height,
		device,
		heap,
		descriptorManager);
}

void Carol::ShadowPass::Draw(ID3D12GraphicsCommandList* cmdList)
{
	Clear(cmdList);
	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	for (int i = 0; i < 2; ++i)
	{
		CullInstances(i, cmdList);
		DrawShadow(i, cmdList);
		GenerateHiZ(cmdList);
	}
}

void Carol::ShadowPass::Update(uint32_t lightIdx, uint64_t cpuFenceValue, uint64_t completedFenceValue)
{
	XMMATRIX view = mCamera->GetView();
	XMMATRIX proj = mCamera->GetProj();
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	
	XMStoreFloat4x4(&mLight->View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mLight->Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mLight->ViewProj, XMMatrixTranspose(viewProj));

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(OPAQUE_MESH_START + i);
		mCulledCommandBufferPool->DiscardBuffer(mCulledCommandBuffer[type].release(), cpuFenceValue);
		mCulledCommandBuffer[type] = mCulledCommandBufferPool->RequestBuffer(completedFenceValue, mScene->GetMeshesCount(type));

		mCullIdx[i][CULL_CULLED_COMMAND_BUFFER_IDX] = mCulledCommandBuffer[type]->GetGpuUavIdx();
		mCullIdx[i][CULL_MESH_COUNT] = mScene->GetMeshesCount(type);
		mCullIdx[i][CULL_MESH_OFFSET] = mScene->GetMeshCBStartOffet(type);
		mCullIdx[i][CULL_HIZ_IDX] = mHiZMap->GetGpuSrvIdx();
		mCullIdx[i][CULL_LIGHT_IDX] = lightIdx;
	}

	mHiZIdx[HIZ_DEPTH_IDX] = mShadowMap->GetGpuSrvIdx();
	mHiZIdx[HIZ_R_IDX] = mHiZMap->GetGpuSrvIdx();
	mHiZIdx[HIZ_W_IDX] = mHiZMap->GetGpuUavIdx();
}

uint32_t Carol::ShadowPass::GetShadowSrvIdx()const
{
	return mShadowMap->GetGpuSrvIdx();
}

const Carol::Light& Carol::ShadowPass::GetLight()const
{
	return *mLight;
}

void Carol::ShadowPass::InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)
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
		device,
		heap,
		descriptorManager,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		&optClearValue);

	mHiZMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mHiZFormat,
		device,
		heap,
		descriptorManager,
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		nullptr,
		mHiZMipLevels);

	mCulledCommandBufferPool = make_unique<StructuredBufferPool>(
		1024,
		sizeof(IndirectCommand),
		device,
		heap,
		descriptorManager,
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(OPAQUE_MESH_START + i);
		mCullIdx[type].resize(CULL_IDX_COUNT);
	}
}

void Carol::ShadowPass::InitLight()
{
	mViewport.TopLeftX = 0.0f;
	mViewport.TopLeftY = 0.0f;
	mViewport.Width = mWidth;
	mViewport.Height = mHeight;
	mViewport.MinDepth = 0.0f;
	mViewport.MaxDepth = 1.0f;
	mScissorRect = { 0,0,(int)mWidth,(int)mHeight };
}

void Carol::ShadowPass::Clear(ID3D12GraphicsCommandList* cmdList)
{
	mScene->ClearCullMark(cmdList);

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(OPAQUE_MESH_START + i);

		mCulledCommandBuffer[type]->Transition(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);
		mCulledCommandBuffer[type]->ResetCounter(cmdList);
	}
}

void Carol::ShadowPass::CullInstances(uint32_t iteration, ID3D12GraphicsCommandList* cmdList)
{
	cmdList->SetPipelineState(gPSOs[L"ShadowInstanceCull"]->Get());
	
	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(OPAQUE_MESH_START + i);

		if (mCullIdx[type][CULL_MESH_COUNT] == 0)
		{
			continue;
		}
		
		mCullIdx[i][CULL_ITERATION] = iteration;
		uint32_t count = ceilf(mCullIdx[type][CULL_MESH_COUNT] / 32.f);

		cmdList->SetComputeRoot32BitConstants(PASS_CONSTANTS, CULL_IDX_COUNT, mCullIdx[i].data(), 0);
		mCulledCommandBuffer[type]->Transition(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		cmdList->Dispatch(count, 1, 1);
		mCulledCommandBuffer[type]->UavBarrier(cmdList);
	}
}

void Carol::ShadowPass::DrawShadow(uint32_t iteration, ID3D12GraphicsCommandList* cmdList)
{
	if (iteration == 0)
	{
		cmdList->ClearDepthStencilView(mShadowMap->GetDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
		cmdList->OMSetRenderTargets(0, nullptr, true, GetRvaluePtr(mShadowMap->GetDsv()));
	}

	ID3D12PipelineState* pso[] = {gPSOs[L"ShadowStatic"]->Get(), gPSOs[L"ShadowSkinned"]->Get()};

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(OPAQUE_MESH_START + i);

		if (mCullIdx[type][CULL_MESH_COUNT] == 0)
		{
			continue;
		}

		mCullIdx[type][CULL_ITERATION] = iteration;

		cmdList->SetPipelineState(pso[i]);
		mCulledCommandBuffer[type]->Transition(cmdList, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		cmdList->SetGraphicsRoot32BitConstants(PASS_CONSTANTS, CULL_IDX_COUNT, mCullIdx[type].data(), 0);
		ExecuteIndirect(cmdList, mCulledCommandBuffer[type].get());
	}
}

void Carol::ShadowPass::GenerateHiZ(ID3D12GraphicsCommandList* cmdList)
{
	cmdList->SetPipelineState(gPSOs[L"HiZGenerate"]->Get());

	for (int i = 0; i < mHiZMipLevels - 1; i += 5)
	{
		mHiZIdx[HIZ_SRC_MIP] = i;
		mHiZIdx[HIZ_NUM_MIP_LEVEL] = i + 5 >= mHiZMipLevels ? mHiZMipLevels - i - 1 : 5;
	    cmdList->SetComputeRoot32BitConstants(PASS_CONSTANTS, HIZ_IDX_COUNT, mHiZIdx.data(), 0);
		
		uint32_t width = ceilf((mWidth >> i) / 32.f);
		uint32_t height = ceilf((mHeight >> i) / 32.f);
		cmdList->Dispatch(width, height, 1);
		mHiZMap->UavBarrier(cmdList);
	}
}

void Carol::ShadowPass::InitShaders()
{
	vector<wstring_view> nullDefines{};

	vector<wstring_view> shadowDefines =
	{
		L"SHADOW"
	};

	vector<wstring_view> skinnedShadowDefines =
	{
		L"SKINNED",
		L"SHADOW"
	};

	vector<wstring_view> shadowCullDefines =
	{
		L"SHADOW",
		L"FRUSTUM",
		L"NORMAL_CONE"
		L"HIZ_OCCLUSION",
		L"WRITE"
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

void Carol::ShadowPass::InitPSOs(ID3D12Device* device)
{
	if (gPSOs.count(L"ShadowStatic") == 0)
	{
		auto shadowStaticMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
		shadowStaticMeshPSO->SetRootSignature(sRootSignature.get());
		shadowStaticMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
		shadowStaticMeshPSO->SetDepthTargetFormat(GetDsvFormat(mShadowFormat));
		shadowStaticMeshPSO->SetAS(gShaders[L"ShadowAS"].get());
		shadowStaticMeshPSO->SetMS(gShaders[L"ShadowStaticMS"].get());
		shadowStaticMeshPSO->Finalize(device);
	
		gPSOs[L"ShadowStatic"] = std::move(shadowStaticMeshPSO);
	}

	if (gPSOs.count(L"ShadowSkinned") == 0)
	{
		auto shadowSkinnedMeshPSO = make_unique<MeshPSO>(PSO_DEFAULT);
		shadowSkinnedMeshPSO->SetRootSignature(sRootSignature.get());
		shadowSkinnedMeshPSO->SetDepthBias(mDepthBias, mDepthBiasClamp, mSlopeScaledDepthBias);
		shadowSkinnedMeshPSO->SetDepthTargetFormat(GetDsvFormat(mShadowFormat));
		shadowSkinnedMeshPSO->SetAS(gShaders[L"ShadowAS"].get());
		shadowSkinnedMeshPSO->SetMS(gShaders[L"ShadowSkinnedMS"].get());
		shadowSkinnedMeshPSO->Finalize(device);
	
		gPSOs[L"ShadowSkinned"] = std::move(shadowSkinnedMeshPSO);
	}

	if (gPSOs.count(L"ShadowInstanceCull") == 0)
	{
		auto shadowInstanceCullComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
		shadowInstanceCullComputePSO->SetRootSignature(sRootSignature.get());
		shadowInstanceCullComputePSO->SetCS(gShaders[L"ShadowCullCS"].get());
		shadowInstanceCullComputePSO->Finalize(device);

		gPSOs[L"ShadowInstanceCull"] = std::move(shadowInstanceCullComputePSO);
	}
}

Carol::DirectLightShadowPass::DirectLightShadowPass(
	ID3D12Device* device,
	Heap* heap,
	DescriptorManager* descriptorManager,
	Scene* scene,
	Light light,
	uint32_t width,
	uint32_t height,
	uint32_t depthBias,
	float depthBiasClamp,
	float slopeScaledDepthBias,
	DXGI_FORMAT shadowFormat,
	DXGI_FORMAT hiZFormat)
	:ShadowPass(
		device,
		heap,
		descriptorManager,
		scene,
		light,
		width,
		height,
		depthBias,
		depthBiasClamp,
		slopeScaledDepthBias,
		shadowFormat,
		hiZFormat)
{
	InitCamera();
}

void Carol::DirectLightShadowPass::Update(
	uint32_t lightIdx,
	const PerspectiveCamera* camera,
	float zn,
	float zf,
	uint64_t cpuFenceValue,
	uint64_t completedFenceValue)
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
	XMMATRIX invPerspOrthoView = XMMatrixMultiply(invPerspView, orthoView);

	for (int i = 0; i < 4; ++i)
	{
		XMFLOAT4 point;
		
		point = { pointNear.x* dx[i], pointNear.y* dy[i], pointNear.z, 1.f };
		XMStoreFloat4(&point, XMVector4Transform(XMLoadFloat4(&point), invPerspOrthoView));
		frustumSliceExtremaPoints.push_back(point);

		point = { pointFar.x* dx[i], pointFar.y* dy[i], pointFar.z, 1.f };
		XMStoreFloat4(&point, XMVector4Transform(XMLoadFloat4(&point), invPerspOrthoView));
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
	mCamera->LookAt(XMLoadFloat4(&center) - 140.f * XMLoadFloat3(&mLight->Direction), XMLoadFloat4(&center), { 0.f,1.f,0.f,0.f });
	mCamera->UpdateViewMatrix();

	XMVECTOR p = { -5.80167627f,0.f,-12.6092882f,1.f };
	p = XMVector4Transform(p, XMMatrixMultiply(mCamera->GetView(), mCamera->GetProj()));

	ShadowPass::Update(lightIdx, cpuFenceValue, completedFenceValue);
}

void Carol::DirectLightShadowPass::InitCamera()
{
	mLight->Position = XMFLOAT3(-mLight->Direction.x * 140.f, -mLight->Direction.y * 140.f, -mLight->Direction.z * 140.f);
	mCamera = make_unique<OrthographicCamera>(70, 0, 280);
	mCamera->LookAt(mLight->Position, { 0.f,0.f,0.f }, { 0.f,1.f,0.f });
	mCamera->UpdateViewMatrix();
}

Carol::CascadedShadowPass::CascadedShadowPass(
	ID3D12Device* device,
	Heap* heap,
	DescriptorManager* descriptorManager,
	Scene* scene,
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
			device,
			heap,
			descriptorManager,
			scene,
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

void Carol::CascadedShadowPass::Draw(ID3D12GraphicsCommandList* cmdList)
{
	for (auto& shadow : mShadow)
	{
		shadow->Draw(cmdList);
	}
}

void Carol::CascadedShadowPass::Update(
	const PerspectiveCamera* camera,
	uint64_t cpuFenceValue,
	uint64_t completedFenceValue,
	float logWeight,
	float bias)
{
	float nearZ = camera->GetNearZ();
	float farZ = camera->GetFarZ();

	for (int i = 0; i < mSplitLevel + 1; ++i)
	{
		mSplitZ[i] = logWeight * nearZ * pow(farZ / nearZ, 1.f * i / mSplitLevel) + (1 - logWeight) * (nearZ + (farZ - nearZ) * (1.f * i / mSplitLevel)) + bias;
	}

	for (int i = 0; i < mSplitLevel; ++i)
	{
		mShadow[i]->Update(i, camera, mSplitZ[i], mSplitZ[i + 1], cpuFenceValue, completedFenceValue);
	}
}

uint32_t Carol::CascadedShadowPass::GetSplitLevel()const
{
	return mSplitLevel;
}

float Carol::CascadedShadowPass::GetSplitZ(uint32_t idx)const
{
	return mSplitZ[idx];
}

uint32_t Carol::CascadedShadowPass::GetShadowSrvIdx(uint32_t idx)const
{
	return mShadow[idx]->GetShadowSrvIdx();
}

const Carol::Light& Carol::CascadedShadowPass::GetLight(uint32_t idx)const
{
	return mShadow[idx]->GetLight();
}

void Carol::CascadedShadowPass::InitShaders()
{
}

void Carol::CascadedShadowPass::InitPSOs(ID3D12Device* device)
{
}

void Carol::CascadedShadowPass::InitBuffers(ID3D12Device* device, Heap* heap, DescriptorManager* descriptorManager)
{
}