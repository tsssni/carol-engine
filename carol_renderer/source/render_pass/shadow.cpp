#include <render_pass/shadow.h>
#include <render_pass/global_resources.h>
#include <render_pass/display.h>
#include <render_pass/mesh.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/shader.h>
#include <dx12/descriptor_allocator.h>
#include <dx12/root_signature.h>
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

Carol::ShadowPass::ShadowPass(GlobalResources* globalResources, Light light, uint32_t width, uint32_t height, DXGI_FORMAT shadowFormat, DXGI_FORMAT shadowDsvFormat, DXGI_FORMAT shadowSrvFormat)
	:RenderPass(globalResources), mLight(make_unique<Light>(light)), mWidth(width), mHeight(height), mShadowFormat(shadowFormat), mShadowDsvFormat(shadowDsvFormat), mShadowSrvFormat(shadowSrvFormat)
{
	InitLightView();
	InitCamera();
	InitShaders();
	InitPSOs();
	InitResources();
}

void Carol::ShadowPass::Draw()
{
	mGlobalResources->CommandList->RSSetViewports(1, &mViewport);
	mGlobalResources->CommandList->RSSetScissorRects(1, &mScissorRect);

	mGlobalResources->CommandList->ClearDepthStencilView(GetDsv(SHADOW_DSV), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mGlobalResources->CommandList->OMSetRenderTargets(0, nullptr, true, GetRvaluePtr(GetDsv(SHADOW_DSV)));
	mGlobalResources->CommandList->SetGraphicsRoot32BitConstant(RootSignature::PASS_CONSTANTS, 0, 0);

	mGlobalResources->Meshes->DrawMeshes(
		{ (*mGlobalResources->PSOs)[L"ShadowStatic"].Get(),
		(*mGlobalResources->PSOs)[L"ShadowSkinned"].Get(),
		(*mGlobalResources->PSOs)[L"ShadowStatic"].Get(),
		(*mGlobalResources->PSOs)[L"ShadowSkinned"].Get() }
	);
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
}

void Carol::ShadowPass::OnResize()
{
}

void Carol::ShadowPass::ReleaseIntermediateBuffers()
{
}

uint32_t Carol::ShadowPass::GetShadowSrvIdx()
{
	return mGpuCbvSrvUavAllocInfo->StartOffset + SHADOW_SRV;
}

const Carol::Light& Carol::ShadowPass::GetLight()
{
	return *mLight;
}

void Carol::ShadowPass::InitResources()
{
	D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = mWidth;
    texDesc.Height = mHeight;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = mShadowFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = mShadowDsvFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	mShadowMap = make_unique<DefaultResource>(&texDesc, mGlobalResources->TexturesHeap, D3D12_RESOURCE_STATE_DEPTH_WRITE, &optClear);
	InitDescriptors();
}

void Carol::ShadowPass::InitDescriptors()
{
	mGlobalResources->CbvSrvUavAllocator->CpuAllocate(SHADOW_CBV_SRV_UAV_COUNT, mCpuCbvSrvUavAllocInfo.get());
	mGlobalResources->DsvAllocator->CpuAllocate(SHADOW_DSV_COUNT, mDsvAllocInfo.get());
	
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mShadowSrvFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	mGlobalResources->Device->CreateShaderResourceView(mShadowMap->Get(), &srvDesc, GetCpuCbvSrvUav(SHADOW_SRV));
	
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = mShadowDsvFormat;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	mGlobalResources->Device->CreateDepthStencilView(mShadowMap->Get(), &dsvDesc, GetDsv(SHADOW_DSV));

	CopyDescriptors();
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
	mCamera = make_unique<OrthographicCamera>(mLight->Direction, DirectX::XMFLOAT3{0.0f,0.0f,0.0f}, 70.0f);
}

void Carol::ShadowPass::InitShaders()
{
	vector<wstring> nullDefines{};

	vector<wstring> skinnedDefines =
	{
		L"SKINNED=1"
	};

	(*mGlobalResources->Shaders)[L"ShadowAS"] = make_unique<Shader>(L"shader\\shadow_as.hlsl", nullDefines, L"main", L"as_6_6");
	(*mGlobalResources->Shaders)[L"ShadowStaticMS"] = make_unique<Shader>(L"shader\\shadow_ms.hlsl", nullDefines, L"main", L"ms_6_6");
	(*mGlobalResources->Shaders)[L"ShadowSkinnedMS"] = make_unique<Shader>(L"shader\\shadow_ms.hlsl", skinnedDefines, L"main", L"ms_6_6");
}

void Carol::ShadowPass::InitPSOs()
{
	auto shadowStaticPsoDesc = *mGlobalResources->BasePsoDesc;
	auto shadowStaticAS = (*mGlobalResources->Shaders)[L"ShadowAS"].get();
	auto shadowStaticMS = (*mGlobalResources->Shaders)[L"ShadowStaticMS"].get();
	shadowStaticPsoDesc.AS = { reinterpret_cast<byte*>(shadowStaticAS->GetBufferPointer()),shadowStaticAS->GetBufferSize() };
	shadowStaticPsoDesc.MS = { reinterpret_cast<byte*>(shadowStaticMS->GetBufferPointer()),shadowStaticMS->GetBufferSize() };
	shadowStaticPsoDesc.NumRenderTargets = 0;
	shadowStaticPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	shadowStaticPsoDesc.DSVFormat = mShadowDsvFormat;
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
}