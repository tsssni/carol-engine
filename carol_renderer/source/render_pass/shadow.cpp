#include <render_pass/shadow.h>
#include <render_pass/global_resources.h>
#include <render_pass/display.h>
#include <render_pass/mesh.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/shader.h>
#include <dx12/descriptor.h>
#include <dx12/root_signature.h>
#include <scene/scene.h>
#include <scene/assimp.h>
#include <scene/camera.h>
#include <utils/common.h>
#include <DirectXColors.h>
#include <memory>

namespace Carol {
	using std::vector;
	using std::wstring;
	using std::unique_ptr;
	using std::make_unique;
	using namespace DirectX;
}

Carol::ShadowPass::ShadowPass(GlobalResources* globalResources, Light light, uint32_t width, uint32_t height, DXGI_FORMAT shadowFormat, DXGI_FORMAT hiZFormat)
	:RenderPass(globalResources),
	mLight(make_unique<Light>(light)),
	mWidth(width),
	mHeight(height),
	mCullIdx(OPAQUE_MESH_TYPE_COUNT),
	mHiZIdx(HIZ_IDX_COUNT),
	mHiZMipLevels(std::max(ceilf(log2f(width)), ceilf(log2f(height)))),
	mCulledCommandBuffer(globalResources->NumFrame),
	mShadowFormat(shadowFormat),
	mHiZFormat(hiZFormat)
{
	InitLightView();
	InitCamera();
	InitShaders();
	InitPSOs();
	InitBuffers();
}

void Carol::ShadowPass::Draw()
{
	Clear();
	mGlobalResources->CommandList->RSSetViewports(1, &mViewport);
	mGlobalResources->CommandList->RSSetScissorRects(1, &mScissorRect);

	DrawHiZ();
	CullMeshes(true);
	DrawShadow(true);

	DrawHiZ();
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

	uint32_t currFrame = *mGlobalResources->CurrFrame;

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		MeshType type = MeshType(OPAQUE_MESH_START + i);
		TestCommandBufferSize(mCulledCommandBuffer[currFrame][type], mGlobalResources->Scene->GetMeshesCount(type));

		mCullIdx[i][CULL_CULLED_COMMAND_BUFFER_IDX] = mCulledCommandBuffer[currFrame][i]->GetGpuUavIdx();
		mCullIdx[i][CULL_MESH_COUNT] = mGlobalResources->Scene->GetMeshesCount(type);
		mCullIdx[i][CULL_MESH_OFFSET] = mGlobalResources->Meshes->GetMeshCBStartOffet(type);
		mCullIdx[i][CULL_HIZ_IDX] = mHiZMap->GetGpuSrvIdx();
		mCullIdx[i][CULL_LIGHT_IDX] = 0;
	}

	mHiZIdx[HIZ_DEPTH_IDX] = mShadowMap->GetGpuSrvIdx();
	mHiZIdx[HIZ_R_IDX] = mHiZMap->GetGpuSrvIdx();
	mHiZIdx[HIZ_W_IDX] = mHiZMap->GetGpuUavIdx();
}

void Carol::ShadowPass::OnResize()
{
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
		mGlobalResources->HeapManager->GetDefaultBuffersHeap(),
		mGlobalResources->DescriptorManager,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		&optClearValue);

	mHiZMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mHiZFormat,
		mGlobalResources->HeapManager->GetDefaultBuffersHeap(),
		mGlobalResources->DescriptorManager,
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		nullptr,
		mHiZMipLevels);

	for (int i = 0; i < mGlobalResources->NumFrame; ++i)
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
	mGlobalResources->Meshes->ClearCullMark();
	uint32_t currFrame = *mGlobalResources->CurrFrame;

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[currFrame][i]->Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COPY_DEST)));
		mCulledCommandBuffer[currFrame][i]->ResetCounter(mGlobalResources->CommandList);
		mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[currFrame][i]->Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)));
	}
}

void Carol::ShadowPass::CullMeshes(bool hist)
{
	uint32_t currFrame = *mGlobalResources->CurrFrame;
	mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"ShadowCull"].Get());
	
	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		if (mCullIdx[i][CULL_MESH_COUNT] == 0)
		{
			continue;
		}
		
		mCullIdx[i][CULL_HIST] = hist;
		uint32_t count = ceilf(mCullIdx[i][CULL_MESH_COUNT] / 32.f);

		mGlobalResources->CommandList->SetComputeRoot32BitConstants(PASS_CONSTANTS, CULL_IDX_COUNT, mCullIdx[i].data(), 0);
		mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[currFrame][i]->Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)));
		mGlobalResources->CommandList->Dispatch(count, 1, 1);
		mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::UAV(mCulledCommandBuffer[currFrame][i]->Get())));
	}
}

void Carol::ShadowPass::DrawShadow(bool hist)
{
	mGlobalResources->CommandList->ClearDepthStencilView(mShadowMap->GetDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mGlobalResources->CommandList->OMSetRenderTargets(0, nullptr, true, GetRvaluePtr(mShadowMap->GetDsv()));

	uint32_t currFrame = *mGlobalResources->CurrFrame;
	ID3D12PipelineState* pso[] = {(*mGlobalResources->PSOs)[L"ShadowStatic"].Get(), (*mGlobalResources->PSOs)[L"ShadowSkinned"].Get()};

	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		if (mCullIdx[i][CULL_MESH_COUNT] == 0)
		{
			continue;
		}

		mCullIdx[i][CULL_HIST] = hist;

		mGlobalResources->CommandList->SetPipelineState(pso[i]);
		mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::Transition(mCulledCommandBuffer[currFrame][i]->Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)));
		mGlobalResources->CommandList->SetGraphicsRoot32BitConstants(PASS_CONSTANTS, CULL_IDX_COUNT, mCullIdx[i].data(), 0);
		mGlobalResources->Meshes->ExecuteIndirect(mCulledCommandBuffer[currFrame][i].get());
	}
}

void Carol::ShadowPass::DrawHiZ()
{
	mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"HiZGenerate"].Get());

	for (int i = 0; i < mHiZMipLevels - 1; i += 5)
	{
		mHiZIdx[HIZ_SRC_MIP] = i;
		mHiZIdx[HIZ_NUM_MIP_LEVEL] = i + 5 >= mHiZMipLevels ? mHiZMipLevels - i - 1 : 5;
		mGlobalResources->CommandList->SetComputeRoot32BitConstants(PASS_CONSTANTS, HIZ_IDX_COUNT, mHiZIdx.data(), 0);
		
		uint32_t width = ceilf((mWidth >> i) / 32.f);
		uint32_t height = ceilf((mHeight >> i) / 32.f);
		mGlobalResources->CommandList->Dispatch(width, height, 1);
		mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::UAV(mHiZMap->Get())));
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
		mGlobalResources->HeapManager->GetDefaultBuffersHeap(),
		mGlobalResources->DescriptorManager,
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}

void Carol::ShadowPass::InitShaders()
{
	vector<wstring> nullDefines{};

	vector<wstring> shadowDefines =
	{
		L"SHADOW=1"
	};

	vector<wstring> skinnedShadowDefines =
	{
		L"SKINNED=1",
		L"SHADOW=1"
	};

	vector<wstring> shadowCullDefines =
	{
		L"SHADOW=1",
		L"OCCLUSION=1",
		L"WRITE=1"
	};

	(*mGlobalResources->Shaders)[L"ShadowCullCS"] = make_unique<Shader>(L"shader\\cull_cs.hlsl", shadowCullDefines, L"main", L"cs_6_6");
	(*mGlobalResources->Shaders)[L"ShadowAS"] = make_unique<Shader>(L"shader\\cull_as.hlsl", shadowCullDefines, L"main", L"as_6_6");
	(*mGlobalResources->Shaders)[L"ShadowStaticMS"] = make_unique<Shader>(L"shader\\depth_ms.hlsl", shadowDefines, L"main", L"ms_6_6");
	(*mGlobalResources->Shaders)[L"ShadowSkinnedMS"] = make_unique<Shader>(L"shader\\depth_ms.hlsl", skinnedShadowDefines, L"main", L"ms_6_6");
}

void Carol::ShadowPass::InitPSOs()
{
	auto shadowStaticPsoDesc = *mGlobalResources->BaseGraphicsPsoDesc;
	auto shadowStaticAS = (*mGlobalResources->Shaders)[L"ShadowAS"].get();
	auto shadowStaticMS = (*mGlobalResources->Shaders)[L"ShadowStaticMS"].get();
	shadowStaticPsoDesc.AS = { reinterpret_cast<byte*>(shadowStaticAS->GetBufferPointer()),shadowStaticAS->GetBufferSize() };
	shadowStaticPsoDesc.MS = { reinterpret_cast<byte*>(shadowStaticMS->GetBufferPointer()),shadowStaticMS->GetBufferSize() };
	shadowStaticPsoDesc.NumRenderTargets = 0;
	shadowStaticPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	shadowStaticPsoDesc.DSVFormat = GetDsvFormat(mShadowFormat);
	shadowStaticPsoDesc.RasterizerState.DepthBias = 60000;
	shadowStaticPsoDesc.RasterizerState.DepthBiasClamp = 0.01f;
	shadowStaticPsoDesc.RasterizerState.SlopeScaledDepthBias = 4.0f;
	auto shadowStaticPsoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(shadowStaticPsoDesc);
    D3D12_PIPELINE_STATE_STREAM_DESC shadowStaticStreamDesc;
    shadowStaticStreamDesc.pPipelineStateSubobjectStream = &shadowStaticPsoStream;
    shadowStaticStreamDesc.SizeInBytes = sizeof(shadowStaticPsoStream);
    ThrowIfFailed(mGlobalResources->Device->CreatePipelineState(&shadowStaticStreamDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"ShadowStatic"].GetAddressOf())));

	auto shadowSkinnedPsoDesc = shadowStaticPsoDesc;
	auto shadowSkinnedMS = (*mGlobalResources->Shaders)[L"ShadowSkinnedMS"].get();
	shadowSkinnedPsoDesc.MS = { reinterpret_cast<byte*>(shadowSkinnedMS->GetBufferPointer()),shadowSkinnedMS->GetBufferSize() };
	auto shadowSkinnedPsoStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(shadowSkinnedPsoDesc);
    D3D12_PIPELINE_STATE_STREAM_DESC shadowSkinnedStreamDesc;
    shadowSkinnedStreamDesc.pPipelineStateSubobjectStream = &shadowSkinnedPsoStream;
    shadowSkinnedStreamDesc.SizeInBytes = sizeof(shadowSkinnedPsoStream);
    ThrowIfFailed(mGlobalResources->Device->CreatePipelineState(&shadowSkinnedStreamDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"ShadowSkinned"].GetAddressOf())));

	auto shadowCullPsoDesc = *mGlobalResources->BaseComputePsoDesc;
	auto shadowCullCS = (*mGlobalResources->Shaders)[L"ShadowCullCS"].get();
	shadowCullPsoDesc.CS = { reinterpret_cast<byte*>(shadowCullCS->GetBufferPointer()), shadowCullCS->GetBufferSize() };
	ThrowIfFailed(mGlobalResources->Device->CreateComputePipelineState(&shadowCullPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"ShadowCull"].GetAddressOf())));
}