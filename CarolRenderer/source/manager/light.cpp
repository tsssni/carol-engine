#include <manager/light.h>
#include <global_resources.h>
#include <manager/display.h>
#include <manager/mesh.h>
#include <dx12/heap.h>
#include <dx12/shader.h>
#include <dx12/descriptor_allocator.h>
#include <scene/assimp.h>
#include <scene/camera.h>
#include <utils/common.h>
#include <DirectXColors.h>
#include <memory>

namespace Carol {
	using std::vector;
	using std::wstring;
	using std::make_unique;
}

Carol::LightManager::LightManager(GlobalResources* globalResources, LightData lightData, uint32_t width, uint32_t height, DXGI_FORMAT shadowFormat, DXGI_FORMAT shadowDsvFormat, DXGI_FORMAT shadowSrvFormat)
	:Manager(globalResources), mLightData(make_unique<LightData>(lightData)), mWidth(width), mHeight(height), mShadowFormat(shadowFormat), mShadowDsvFormat(shadowDsvFormat), mShadowSrvFormat(shadowSrvFormat)
{
	InitConstants();
	InitLightView();
	InitCamera();
	InitShaders();
	InitPSOs();
	InitResources();
}

void Carol::LightManager::Draw()
{
	mGlobalResources->CommandList->RSSetViewports(1, &mViewport);
	mGlobalResources->CommandList->RSSetScissorRects(1, &mScissorRect);

	mGlobalResources->CommandList->ClearDepthStencilView(GetDsv(0), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mGlobalResources->CommandList->OMSetRenderTargets(0, nullptr, true, GetRvaluePtr(GetDsv(0)));

	mGlobalResources->CommandList->SetGraphicsRootSignature(mGlobalResources->RootSignature);
	mGlobalResources->CommandList->SetGraphicsRootConstantBufferView(2, ShadowCBHeap->GetGPUVirtualAddress(mShadowCBAllocInfo.get()));

	DrawAllMeshes();
}

void Carol::LightManager::Update()
{
	DirectX::XMMATRIX view = mCamera->GetView();
	DirectX::XMMATRIX proj = mCamera->GetProj();
	DirectX::XMMATRIX lightViewProj = DirectX::XMMatrixMultiply(view, proj);
	static DirectX::XMMATRIX tex(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f
	);

	DirectX::XMStoreFloat4x4(&mShadowConstants->LightViewProj, DirectX::XMMatrixTranspose(lightViewProj));
	DirectX::XMStoreFloat4x4(&mLightData->ViewProjTex, DirectX::XMMatrixTranspose(DirectX::XMMatrixMultiply(lightViewProj, tex)));

	ShadowCBHeap->DeleteResource(mShadowCBAllocInfo.get());
	ShadowCBHeap->CreateResource(nullptr, nullptr, mShadowCBAllocInfo.get());
	ShadowCBHeap->CopyData(mShadowCBAllocInfo.get(), mShadowConstants.get());
}

void Carol::LightManager::OnResize()
{
}

void Carol::LightManager::ReleaseIntermediateBuffers()
{
}

void Carol::LightManager::InitShadowCBHeap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
	if (!ShadowCBHeap)
	{
		ShadowCBHeap = make_unique<CircularHeap>(device, cmdList, true, 32, sizeof(ShadowConstants));
	}
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::LightManager::GetShadowSrv()
{
	return GetShaderGpuSrv(SHADOW_SRV);
}

const Carol::LightData& Carol::LightManager::GetLightData()
{
	return *mLightData;
}

void Carol::LightManager::InitResources()
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

void Carol::LightManager::InitDescriptors()
{
	mCpuCbvSrvUavAllocInfo = make_unique<DescriptorAllocInfo>();
	mDsvAllocInfo = make_unique<DescriptorAllocInfo>();
	mGlobalResources->CbvSrvUavAllocator->CpuAllocate(LIGHT_SRV_COUNT, mCpuCbvSrvUavAllocInfo.get());
	mGlobalResources->DsvAllocator->CpuAllocate(1, mDsvAllocInfo.get());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = mShadowSrvFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	
    mGlobalResources->Device->CreateShaderResourceView(mShadowMap->Get(), &srvDesc, GetCpuSrv(SHADOW_SRV));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = mShadowDsvFormat;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	mGlobalResources->Device->CreateDepthStencilView(mShadowMap->Get(), &dsvDesc, GetDsv(0));
}

void Carol::LightManager::InitConstants()
{
	mShadowConstants = make_unique<ShadowConstants>();
	mShadowCBAllocInfo = make_unique<HeapAllocInfo>();
}

void Carol::LightManager::InitLightView()
{
	mViewport.TopLeftX = 0.0f;
	mViewport.TopLeftY = 0.0f;
	mViewport.Width = mWidth;
	mViewport.Height = mHeight;
	mViewport.MinDepth = 0.0f;
	mViewport.MaxDepth = 1.0f;
	mScissorRect = { 0,0,(int)mWidth,(int)mHeight };
}

void Carol::LightManager::InitCamera()
{
	mCamera = make_unique<OrthographicCamera>(mLightData->Direction, DirectX::XMFLOAT3{0.0f,0.0f,0.0f}, 70.0f);
}

void Carol::LightManager::DrawAllMeshes()
{
	static vector<MeshManager*> staticMeshes;
	static vector<MeshManager*> skinnedMeshes;
	staticMeshes.clear();
	skinnedMeshes.clear();

	for (auto& modelMapPair : *mGlobalResources->Models)
	{
		for (auto& meshMapPair : modelMapPair.second->GetMeshes())
		{
			auto& mesh = meshMapPair.second;

			if (mesh->IsSkinned())
			{
				skinnedMeshes.push_back(mesh.get());
			}
			else
			{
				staticMeshes.push_back(mesh.get());
			}
		}
	}

	mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"ShadowStatic"].Get());
	for (auto* mesh : staticMeshes)
	{
		mesh->SetTextureDrawing(false);
		mesh->Draw();
	}

	mGlobalResources->CommandList->SetPipelineState((*mGlobalResources->PSOs)[L"ShadowSkinned"].Get());
	for (auto* mesh : skinnedMeshes)
	{
		mesh->SetTextureDrawing(false);
		mesh->Draw();
	}
}

void Carol::LightManager::InitRootSignature()
{
}

void Carol::LightManager::InitShaders()
{
	vector<wstring> nullDefines{};

	vector<wstring> skinnedDefines =
	{
		L"SKINNED=1"
	};

	(*mGlobalResources->Shaders)[L"ShadowStaticVS"] = make_unique<Shader>(L"shader\\shadow.hlsl", nullDefines, L"VS", L"vs_6_5");
	(*mGlobalResources->Shaders)[L"ShadowSkinnedVS"] = make_unique<Shader>(L"shader\\shadow.hlsl", skinnedDefines, L"VS", L"vs_6_5");
}

void Carol::LightManager::InitPSOs()
{
	auto shadowStaticPsoDesc = *mGlobalResources->BasePsoDesc;
	auto shadowStaticVS = (*mGlobalResources->Shaders)[L"ShadowStaticVS"].get();
	shadowStaticPsoDesc.VS = { reinterpret_cast<byte*>(shadowStaticVS->GetBufferPointer()),shadowStaticVS->GetBufferSize() };
	shadowStaticPsoDesc.NumRenderTargets = 0;
	shadowStaticPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	shadowStaticPsoDesc.DSVFormat = mShadowDsvFormat;
	shadowStaticPsoDesc.RasterizerState.DepthBias = 60000;
	shadowStaticPsoDesc.RasterizerState.DepthBiasClamp = 0.01f;
	shadowStaticPsoDesc.RasterizerState.SlopeScaledDepthBias = 4.0f;
	ThrowIfFailed(mGlobalResources->Device->CreateGraphicsPipelineState(&shadowStaticPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"ShadowStatic"].GetAddressOf())));

	auto shadowSkinnedPsoDesc = shadowStaticPsoDesc;
	auto shadowSkinnedVS = (*mGlobalResources->Shaders)[L"ShadowSkinnedVS"].get();
	shadowSkinnedPsoDesc.VS = { reinterpret_cast<byte*>(shadowSkinnedVS->GetBufferPointer()),shadowSkinnedVS->GetBufferSize() };
	ThrowIfFailed(mGlobalResources->Device->CreateGraphicsPipelineState(&shadowSkinnedPsoDesc, IID_PPV_ARGS((*mGlobalResources->PSOs)[L"ShadowSkinned"].GetAddressOf())));
}