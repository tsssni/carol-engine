#include <render_pass/shadow.h>
#include <render_pass/global_resources.h>
#include <render_pass/display.h>
#include <render_pass/mesh.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/shader.h>
#include <dx12/descriptor_allocator.h>
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
	using std::make_unique;
	using namespace DirectX;
}

Carol::ShadowPass::ShadowPass(GlobalResources* globalResources, Light light, uint32_t width, uint32_t height, DXGI_FORMAT shadowFormat, DXGI_FORMAT hiZFormat)
	:RenderPass(globalResources),
	mLight(make_unique<Light>(light)),
	mWidth(width),
	mHeight(height),
	mHiZMipLevels(std::max(floor(log2(width)), floor(log2(height)))),
	mOcclusionCommandBuffer(OPAQUE_MESH_TYPE_COUNT),
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
	//mGlobalResources->Meshes->ClearCullMark();

	//DrawHiZ();
	//CullMeshes();
	//DrawShadow();

	//DrawHiZ();
	//CullMeshes();
	DrawShadow();
}

void Carol::ShadowPass::Update()
{
	UpdateLight();
	UpdateCommandBuffer();
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

void Carol::ShadowPass::CullMeshes()
{
	uint32_t currFrame = *mGlobalResources->CurrFrame;
	mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"Cull"].Get());
	
	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		uint32_t count = mGlobalResources->Scene->GetMeshesCount(MeshType(OPAQUE_MESH_START + i));
		count = ceilf(count / 32.f);

		uint32_t cullIdx[] =
		{
			mGlobalResources->Meshes->GetCommandBufferIdx((MeshType)(OPAQUE_MESH_START + i)),
			mOcclusionCommandBuffer[i]->GetGpuUavIdx(),
			mGlobalResources->Meshes->GetCullMarkIdx((MeshType)(OPAQUE_MESH_START + i)),
			mHiZMap->GetGpuSrvIdx(),
			mGlobalResources->Scene->GetMeshesCount(MeshType(OPAQUE_MESH_START + i)),
			0
		};

		mGlobalResources->CommandList->Dispatch(count, 1, 1);
		mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::UAV(mOcclusionCommandBuffer[i]->Get())));
	}
}

void Carol::ShadowPass::DrawShadow()
{
	mGlobalResources->CommandList->RSSetViewports(1, &mViewport);
	mGlobalResources->CommandList->RSSetScissorRects(1, &mScissorRect);

	mGlobalResources->CommandList->ClearDepthStencilView(mShadowMap->GetDsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mGlobalResources->CommandList->OMSetRenderTargets(0, nullptr, true, GetRvaluePtr(mShadowMap->GetDsv()));

	uint32_t constants[] = { 0,mHiZMap->GetGpuSrvIdx() };
	mGlobalResources->CommandList->SetGraphicsRoot32BitConstants(RootSignature::PASS_CONSTANTS, 2, constants, 0);

	ID3D12PipelineState* pso[] = {(*mGlobalResources->PSOs)[L"ShadowStatic"].Get(), (*mGlobalResources->PSOs)[L"ShadowSkinned"].Get()};
	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		/*uint32_t numElements = mGlobalResources->Scene->GetMeshesCount(Scene::OPAQUE_MESH_START + i);
		uint32_t stride = sizeof(IndirectCommand);
		uint32_t size = numElements * stride;
		uint32_t offset = ceilf(size / 4096.f) * 4096;

		mGlobalResources->CommandList->SetPipelineState(pso[i]);
		mGlobalResources->CommandList->ExecuteIndirect(
			mGlobalResources->Meshes->GetCommandSignature(),
			mGlobalResources->Scene->GetMeshesCount(Scene::OPAQUE_MESH_START + i),
			mOcclusionCommandBuffer[i]->Get(),
			0,
			mOcclusionCommandBuffer[i]->Get(),
			offset
		);*/
	}
}

void Carol::ShadowPass::DrawHiZ()
{
	mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"HiZGenerate"].Get());

	for (int i = 0; i < mHiZMipLevels; i += 5)
	{
		uint32_t hiZConstants[] = { 
			mShadowMap->GetGpuSrvIdx(),
			mHiZMap->GetGpuSrvIdx(),
			mHiZMap->GetGpuUavIdx(),
			i,
			i + 5 >= mHiZMipLevels ? mHiZMipLevels - i - 1 : 5,
			mWidth,
			mHeight};
		mGlobalResources->CommandList->SetComputeRoot32BitConstants(RootSignature::PASS_CONSTANTS, _countof(hiZConstants), hiZConstants, 0);
		
		uint32_t width = ceilf((mWidth >> i) / 32.f);
		uint32_t height = ceilf((mHeight >> i) / 32.f);
		mGlobalResources->CommandList->Dispatch(width, height, 1);
		mGlobalResources->CommandList->ResourceBarrier(1, GetRvaluePtr(CD3DX12_RESOURCE_BARRIER::UAV(mHiZMap->Get())));
	}
}

void Carol::ShadowPass::UpdateLight()
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
}

void Carol::ShadowPass::UpdateCommandBuffer()
{
	for (int i = 0; i < OPAQUE_MESH_TYPE_COUNT; ++i)
	{
		uint32_t numElements = mGlobalResources->Scene->GetMeshesCount(MeshType(OPAQUE_MESH_START + i));

		if (numElements > 0)
		{
			uint32_t stride = sizeof(IndirectCommand);
			mOcclusionCommandBuffer[i].reset();
			mOcclusionCommandBuffer[i] = make_unique<StructuredBuffer>(
				numElements,
				stride,
				mGlobalResources->HeapManager->GetDefaultBuffersHeap(),
				mGlobalResources->DescriptorManager,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		}
	}
}

void Carol::ShadowPass::InitShaders()
{
	vector<wstring> nullDefines{};

	vector<wstring> shadowDefines =
	{
		L"SHADOW=1"
	};

	vector<wstring> skinnedDefines =
	{
		L"SKINNED=1"
	};

	(*mGlobalResources->Shaders)[L"ShadowAS"] = make_unique<Shader>(L"shader\\cull_as.hlsl", shadowDefines, L"main", L"as_6_6");
	(*mGlobalResources->Shaders)[L"ShadowStaticMS"] = make_unique<Shader>(L"shader\\shadow_ms.hlsl", nullDefines, L"main", L"ms_6_6");
	(*mGlobalResources->Shaders)[L"ShadowSkinnedMS"] = make_unique<Shader>(L"shader\\shadow_ms.hlsl", skinnedDefines, L"main", L"ms_6_6");
	(*mGlobalResources->Shaders)[L"ShadowCullCS"] = make_unique<Shader>(L"shader\\cull_cs.hlsl", shadowDefines, L"main", L"cs_6_6");
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