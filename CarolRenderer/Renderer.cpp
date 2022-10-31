#include "Renderer.h"
#include "Resource/Light.h"
#include "Utils/Common.h"
#include <DirectXColors.h>
using std::make_unique;
using namespace DirectX;

const int gNumFramesResources = 3;

void Carol::Renderer::InitRenderer(HWND hWnd, uint32_t width, uint32_t height)
{
	mSsao = make_unique<SsaoManager>();
	mTaa = make_unique<TaaManager>();
	mMaterialBuffer = make_unique<DefaultBuffer>();

	BaseRenderer::InitRenderer(hWnd, width, height);
	ThrowIfFailed(mInitCommandAllocator->Reset());
	ThrowIfFailed(mCommandList->Reset(mInitCommandAllocator.Get(), nullptr));

	mCamera->SetPosition(0.0f, 0.0f, -5.0f);
	mSsao->InitSsaoManager(mDevice.Get(), mCommandList.Get(), mClientWidth, mClientHeight);
	mTaa->SetFrameFormat(mDisplayManager->GetBackBufferFormat());
	mTaa->InitTaa(mDevice.Get(), mClientWidth, mClientHeight);

	InitRootSignature();
	InitSsaoRootSignature();
	InitModels();
	InitCbvSrvUavDescriptorHeaps();
	InitShaders();
	InitRenderItems();
	InitFrameResources();
	InitPSOs();

	mSsao->SetPSOs(mPSOs[L"ssao"].Get(), mPSOs[L"ssaoBlur"].Get());
	mCommandList->Close();
	
	vector<ID3D12CommandList*> cmdLists{ mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(1, cmdLists.data());
	FlushCommandQueue();
}

void Carol::Renderer::InitRootSignature()
{
	CD3DX12_ROOT_PARAMETER rootPara[6];
	rootPara[0].InitAsConstantBufferView(0);
	rootPara[1].InitAsConstantBufferView(1);
	rootPara[2].InitAsConstantBufferView(2);

	CD3DX12_DESCRIPTOR_RANGE range[3];
	range[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 256, 0);
	range[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 1);
	range[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 1);

	rootPara[3].InitAsDescriptorTable(1, &range[0], D3D12_SHADER_VISIBILITY_PIXEL);
	rootPara[4].InitAsDescriptorTable(1, &range[1], D3D12_SHADER_VISIBILITY_PIXEL);
	rootPara[5].InitAsDescriptorTable(1, &range[2], D3D12_SHADER_VISIBILITY_PIXEL);
	
	auto staticSamplers = GetDefaultStaticSamplers();
	CD3DX12_ROOT_SIGNATURE_DESC slotRootSigDesc(6, rootPara, staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	Blob serializedRootSigBlob;
	Blob errorBlob;

	auto hr = D3D12SerializeRootSignature(&slotRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSigBlob.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob.Get())
	{
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	
	ThrowIfFailed(hr);
	ThrowIfFailed(mDevice->CreateRootSignature(0, serializedRootSigBlob->GetBufferPointer(), serializedRootSigBlob->GetBufferSize(), IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void Carol::Renderer::InitSsaoRootSignature()
{
	CD3DX12_ROOT_PARAMETER rootPara[4];
	rootPara[0].InitAsConstantBufferView(0);
	rootPara[1].InitAsConstants(1, 1);

	CD3DX12_DESCRIPTOR_RANGE range[2];
	range[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
	range[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

	rootPara[2].InitAsDescriptorTable(1, &range[0], D3D12_SHADER_VISIBILITY_PIXEL);
	rootPara[3].InitAsDescriptorTable(1, &range[1], D3D12_SHADER_VISIBILITY_PIXEL);

	auto staticSamplers = GetDefaultStaticSamplers();
	CD3DX12_ROOT_SIGNATURE_DESC slotRootSigDesc(4, rootPara, staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	Blob serializedRootSigBlob;
	Blob errorBlob;

	auto hr = D3D12SerializeRootSignature(&slotRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSigBlob.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob.Get())
	{
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}

	ThrowIfFailed(hr);
	ThrowIfFailed(mDevice->CreateRootSignature(0, serializedRootSigBlob->GetBufferPointer(), serializedRootSigBlob->GetBufferSize(), IID_PPV_ARGS(mSsaoRootSignature.GetAddressOf())));

}

vector<CD3DX12_STATIC_SAMPLER_DESC>& Carol::Renderer::GetDefaultStaticSamplers()
{
	static CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0,
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP);

	static CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1,
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	static CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP);

	static CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	static CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4,
		D3D12_FILTER_ANISOTROPIC,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		0.0f,
		8);

	static CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5,
		D3D12_FILTER_ANISOTROPIC,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		0.0f,
		8);

	static vector<CD3DX12_STATIC_SAMPLER_DESC> defaultSamplers {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp
	};

	return defaultSamplers;
}

void Carol::Renderer::InitRtvDsvDescriptorHeaps()
{
	mRtvHeap = make_unique<RtvDescriptorHeap>();
	mDsvHeap = make_unique<DsvDescriptorHeap>();

	// 2 for swap chain buffers
	// 1 for SSAO normal map
	// 2 for SSAO ambient map
	// 1 for TAA currframe map
	// 1 for TAA velocity map
	uint32_t numRtvs = mDisplayManager->GetBackBufferCount() + mSsao->GetNumRtvs() + mTaa->GetNumRtvs();
	uint32_t numDsvs = 1;

	mRtvHeap->InitRtvDescriptorHeap(mDevice.Get(), numRtvs);
	mDsvHeap->InitDsvDescriptorHeap(mDevice.Get(), numDsvs);
}

void Carol::Renderer::InitCbvSrvUavDescriptorHeaps()
{
	mCbvSrvUavHeap = make_unique<CbvSrvUavDescriptorHeap>();
	mCbvSrvUavHeap->InitCbvSrvUavDescriptorHeap(mDevice.Get(), 64);

	uint32_t texSrvIndex = 0;
	uint32_t matSrvIndex = mTextures.size();

	uint32_t ssaoSrvIndex = matSrvIndex + 1;
	uint32_t ssaoRtvIndex = mDisplayManager->GetBackBufferCount();

	uint32_t taaSrvIndex = ssaoSrvIndex + mSsao->GetNumSrvs();
	uint32_t taaRtvIndex = ssaoRtvIndex + mSsao->GetNumRtvs();

	InitTexturesDescriptors(texSrvIndex);
	InitMaterialBufferDescriptor(matSrvIndex);

	mSsao->InitDescriptors(
		mDisplayManager->GetDepthStencilBuffer(),
		mCbvSrvUavHeap->GetCpuDescriptor(ssaoSrvIndex),
		mCbvSrvUavHeap->GetGpuDescriptor(ssaoSrvIndex),
		mRtvHeap->GetCpuDescriptor(ssaoRtvIndex),
		mCbvSrvUavHeap->GetDescriptorSize(),
		mRtvHeap->GetDescriptorSize());

	mTaa->InitDescriptors(
		mDisplayManager->GetDepthStencilBuffer(),
		mCbvSrvUavHeap->GetCpuDescriptor(taaSrvIndex),
		mCbvSrvUavHeap->GetGpuDescriptor(taaSrvIndex),
		mRtvHeap->GetCpuDescriptor(taaRtvIndex),
		mCbvSrvUavHeap->GetDescriptorSize(),
		mRtvHeap->GetDescriptorSize());
}


void Carol::Renderer::InitTexturesDescriptors(uint32_t startIndex)
{
	for (auto& texIndexPair : mTexturesIndex)
	{
		auto& texture = mTextures[texIndexPair.first];
		auto index = texIndexPair.second;
		mDevice->CreateShaderResourceView(texture->GetBuffer()->Get(), GetRvaluePtr(texture->GetDesc()), mCbvSrvUavHeap->GetCpuDescriptor(startIndex + index));
	}

	mTexturesSrv = mCbvSrvUavHeap->GetGpuDescriptor(startIndex);
}

void Carol::Renderer::InitMaterialBufferDescriptor(uint32_t startIndex)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = mMaterialData.size();
	srvDesc.Buffer.StructureByteStride = sizeof(MaterialData);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	mDevice->CreateShaderResourceView(mMaterialBuffer->Get(), &srvDesc, mCbvSrvUavHeap->GetCpuDescriptor(startIndex));

	mMaterialBufferSrv = mCbvSrvUavHeap->GetGpuDescriptor(startIndex);
}

void Carol::Renderer::InitModels()
{
	uint32_t texCount = 0;

	auto assimpModel = make_unique<SkinnedAssimpModel>();
	assimpModel->LoadSkinnedAssimpModel(mDevice.Get(), mCommandList.Get(), L"Models\\egyptian_B.fbx", mMaterialData.size(), texCount);
	InitMaterials(assimpModel->GetMaterials());
	InitTextures(assimpModel->GetTextures(), L"Textures");

	auto flatGroundModel = AssimpModel::GetFlatGround(mDevice.Get(), mCommandList.Get(), mMaterialData.size(), texCount);
	InitMaterials(flatGroundModel->GetMaterials());
	InitTextures(flatGroundModel->GetTextures(), L"Textures");

	InitMaterialBuffer();

	mSkinnedInfo[L"Egypt"] = std::make_unique<SkinnedData>(std::move(assimpModel->GetSkinnedData()));
	mModels[L"Egypt"] = std::move(assimpModel);
	mModels[L"FlatGround"] = std::move(flatGroundModel);

	auto assimpSkinnedInstance = make_unique<SkinnedModelInfo>();
	assimpSkinnedInstance->SkinnedInfo = mSkinnedInfo[L"Egypt"].get();
	assimpSkinnedInstance->ClipName = L"Take 001";
	assimpSkinnedInstance->FinalTransforms.resize(300);
	mSkinnedInstances[L"Egypt"] = std::move(assimpSkinnedInstance);
}

void Carol::Renderer::InitMaterials(vector<AssimpMaterial>& mats)
{
	mMaterialData.resize(mMaterialData.size() + mats.size());

	for (int i = 0; i < mats.size(); ++i)
	{
		uint32_t index = mats[i].MatTBIndex;
		mMaterialData[index].DiffuseAlbedo = { mats[i].Diffuse.x,mats[i].Diffuse.y,mats[i].Diffuse.z,1.0f };
		mMaterialData[index].FresnelR0 = mats[i].Reflection;
		mMaterialData[index].Roughness = 1.0f - mats[i].Shininess;
		XMStoreFloat4x4(&mMaterialData[index].MatTransform, XMMatrixIdentity());
		mMaterialData[index].DiffuseSrvHeapIndex = mats[i].DiffuseMapIndex;
		mMaterialData[index].NormalSrvHeapIndex = mats[i].NormalMapIndex;
	}
}

void Carol::Renderer::InitMaterialBuffer()
{
	mMaterialBuffer->InitDefaultBuffer(mDevice.Get(), D3D12_HEAP_FLAG_NONE, GetRvaluePtr(CD3DX12_RESOURCE_DESC::Buffer(mMaterialData.size() * sizeof(MaterialData))));

	D3D12_SUBRESOURCE_DATA subresource;
	subresource.pData = mMaterialData.data();
	subresource.RowPitch = sizeof(MaterialData) * mMaterialData.size();
	subresource.SlicePitch = subresource.RowPitch;

	mMaterialBuffer->CopySubresources(mCommandList.Get(), &subresource);
}

void Carol::Renderer::InitTextures(unordered_map<wstring, uint32_t>& texIndexMap, wstring dirPath)
{
	for (auto& texturePair : texIndexMap)
	{
		wstring fileName = texturePair.first;
		uint32_t index = texturePair.second;

		wstring path = dirPath + L'\\' + fileName;
		mTextures[fileName] = make_unique<Texture>();
		mTextures[fileName]->LoadTexture(path, mDevice.Get(), mCommandList.Get());
		mTexturesIndex[fileName] = index;
	}
}

void Carol::Renderer::InitShaders()
{
	auto VS = make_unique<Shader>();
	auto PS = make_unique<Shader>();
	auto SkinnedVS = make_unique<Shader>();
	auto DrawNormalsVS = make_unique<Shader>();
	auto DrawNormalsPS = make_unique<Shader>();
	auto ssaoVS = std::make_unique<Shader>();
	auto ssaoPS = std::make_unique<Shader>();
	auto ssaoBlurVS = std::make_unique<Shader>();
	auto ssaoBlurPS = std::make_unique<Shader>();
	auto taaVelocityVS = std::make_unique<Shader>();
	auto taaVelocityPS = std::make_unique<Shader>();
	auto taaOutputVS = std::make_unique<Shader>();
	auto taaOutputPS = std::make_unique<Shader>();

	const D3D_SHADER_MACRO skinnedDefines[] =
	{
		"SKINNED", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO lightDefines[] =
	{
		"NUM_DIR_LIGHTS","3"
	};

	VS->CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	PS->CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");
	SkinnedVS->CompileShader(L"Shaders\\Default.hlsl", skinnedDefines, "VS", "vs_5_1");
	DrawNormalsVS->CompileShader(L"Shaders\\DrawNormals.hlsl", nullptr, "VS", "vs_5_1");
	DrawNormalsPS->CompileShader(L"Shaders\\DrawNormals.hlsl", nullptr, "PS", "ps_5_1");
	ssaoVS->CompileShader(L"Shaders\\Ssao.hlsl", nullptr, "VS", "vs_5_1");
	ssaoPS->CompileShader(L"Shaders\\Ssao.hlsl", nullptr, "PS", "ps_5_1");
	ssaoBlurVS->CompileShader(L"Shaders\\SsaoBlur.hlsl", nullptr, "VS", "vs_5_1");
	ssaoBlurPS->CompileShader(L"Shaders\\SsaoBlur.hlsl", nullptr, "PS", "ps_5_1");
	taaVelocityVS->CompileShader(L"Shaders\\Velocity.hlsl", nullptr, "VS", "vs_5_1");
	taaVelocityPS->CompileShader(L"Shaders\\Velocity.hlsl", nullptr, "PS", "ps_5_1");
	taaOutputVS->CompileShader(L"Shaders\\Taa.hlsl", nullptr, "VS", "vs_5_1");
	taaOutputPS->CompileShader(L"Shaders\\Taa.hlsl", nullptr, "PS", "ps_5_1");

	int a = VS->GetBlob()->Get()->GetBufferSize();

	mShaders[L"VS"] = std::move(VS);
	mShaders[L"PS"] = std::move(PS);
	mShaders[L"SkinnedVS"] = std::move(SkinnedVS);
	mShaders[L"DrawNormalsVS"] = std::move(DrawNormalsVS);
	mShaders[L"DrawNormalsPS"] = std::move(DrawNormalsPS);
	mShaders[L"SsaoVS"] = std::move(ssaoVS);
	mShaders[L"SsaoPS"] = std::move(ssaoPS);
	mShaders[L"SsaoBlurVS"] = std::move(ssaoBlurVS);
	mShaders[L"SsaoBlurPS"] = std::move(ssaoBlurPS);
	mShaders[L"TaaVelocityVS"] = std::move(taaVelocityVS);
	mShaders[L"TaaVelocityPS"] = std::move(taaVelocityPS);
	mShaders[L"TaaOutputVS"] = std::move(taaOutputVS);
	mShaders[L"TaaOutputPS"] = std::move(taaOutputPS);

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "BONEINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	mNullInputLayout.resize(0);
}

void Carol::Renderer::InitRenderItems()
{
	auto& egyptSubmeshes = mModels[L"Egypt"]->GetSubmeshes();
	uint32_t objCBIndex = 0;

	for (auto& submeshPair : egyptSubmeshes)
	{
		auto& submesh = submeshPair.second;
		auto ritem = make_unique<RenderItem>();

		XMStoreFloat4x4(&ritem->World, XMMatrixIdentity());
		XMStoreFloat4x4(&ritem->HistWorld, XMMatrixIdentity());
		XMStoreFloat4x4(&ritem->TexTransform, XMMatrixIdentity());
		ritem->Model = mModels[L"Egypt"].get();
		ritem->BaseVertexLocation = submesh.BaseVertexLocation;
		ritem->StartIndexLocation = submesh.StartIndexLocation;
		ritem->IndexCount = submesh.IndexCount;
		ritem->ObjCBIndex = objCBIndex++;
		ritem->SkinnedCBIndex = 0;
		ritem->MatTBIndex = submesh.MatTBIndex;

		mOpaqueItems.push_back(ritem.get());
		mRenderItems.push_back(std::move(ritem));
	}

	auto& flatGroundSubmesh = mModels[L"FlatGround"]->GetSubmesh(L"FlatGround");
	auto flatGroundRitem = make_unique<RenderItem>();

	XMStoreFloat4x4(&flatGroundRitem->World, XMMatrixIdentity());
	XMStoreFloat4x4(&flatGroundRitem->HistWorld, XMMatrixIdentity());
	XMStoreFloat4x4(&flatGroundRitem->TexTransform, XMMatrixIdentity());
	flatGroundRitem->Model = mModels[L"FlatGround"].get();
	flatGroundRitem->BaseVertexLocation = flatGroundSubmesh.BaseVertexLocation;
	flatGroundRitem->StartIndexLocation = flatGroundSubmesh.StartIndexLocation;
	flatGroundRitem->IndexCount = flatGroundSubmesh.IndexCount;
	flatGroundRitem->ObjCBIndex = objCBIndex++;
	flatGroundRitem->SkinnedCBIndex = -1;
	flatGroundRitem->MatTBIndex = flatGroundSubmesh.MatTBIndex;

	mOpaqueItems.push_back(flatGroundRitem.get());
	mRenderItems.push_back(std::move(flatGroundRitem));
}

void Carol::Renderer::InitFrameResources()
{
	for (int i = 0; i < gNumFramesResources; ++i)
	{
		mFrameResources.push_back(make_unique<FrameResource>(mDevice.Get(), 1, mRenderItems.size(), mSkinnedInstances.size()));
	}
}

void Carol::Renderer::InitPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC basePsoDesc = {};
	basePsoDesc.InputLayout = { mInputLayout.data(),(uint32_t)mInputLayout.size() };
	basePsoDesc.pRootSignature = mRootSignature.Get();
	basePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    basePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    basePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    basePsoDesc.SampleMask = UINT_MAX;
    basePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    basePsoDesc.NumRenderTargets = 1;
    basePsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    basePsoDesc.SampleDesc.Count = 1;
    basePsoDesc.SampleDesc.Quality = 0;
    basePsoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	auto opaquePsoDesc = basePsoDesc;
	auto opaqueVSBlob = mShaders[L"VS"]->GetBlob()->Get();
	auto opaquePSBlob = mShaders[L"PS"]->GetBlob()->Get();
	opaquePsoDesc.VS = { reinterpret_cast<byte*>(opaqueVSBlob->GetBufferPointer()),opaqueVSBlob->GetBufferSize() };
	opaquePsoDesc.PS = { reinterpret_cast<byte*>(opaquePSBlob->GetBufferPointer()),opaquePSBlob->GetBufferSize() };
	ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(mPSOs[L"opaque"].GetAddressOf())));

	auto skinnedPsoDesc = basePsoDesc;
	auto skinnedVSBlob = mShaders[L"SkinnedVS"]->GetBlob()->Get();
	auto skinnedPSBlob = mShaders[L"PS"]->GetBlob()->Get();
	skinnedPsoDesc.VS = { reinterpret_cast<byte*>(skinnedVSBlob->GetBufferPointer()),skinnedVSBlob->GetBufferSize() };
	skinnedPsoDesc.PS = { reinterpret_cast<byte*>(skinnedPSBlob->GetBufferPointer()),skinnedPSBlob->GetBufferSize() };	
	ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&skinnedPsoDesc, IID_PPV_ARGS(mPSOs[L"skinned"].GetAddressOf())));

	auto drawNormalsPsoDesc = basePsoDesc;
	auto drawNormalsVSBlob = mShaders[L"DrawNormalsVS"]->GetBlob()->Get();
	auto drawNormalsPSBlob = mShaders[L"DrawNormalsPS"]->GetBlob()->Get();
	drawNormalsPsoDesc.VS = { reinterpret_cast<byte*>(drawNormalsVSBlob->GetBufferPointer()),drawNormalsVSBlob->GetBufferSize() };
	drawNormalsPsoDesc.PS = { reinterpret_cast<byte*>(drawNormalsPSBlob->GetBufferPointer()),drawNormalsPSBlob->GetBufferSize() };
	drawNormalsPsoDesc.RTVFormats[0] = mSsao->GetNormalMapFormat();
	ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&drawNormalsPsoDesc, IID_PPV_ARGS(mPSOs[L"drawNormals"].GetAddressOf())));
	
	auto ssaoPsoDesc = basePsoDesc;
	ssaoPsoDesc.pRootSignature = mSsaoRootSignature.Get();
	auto ssaoVSBlob = mShaders[L"SsaoVS"]->GetBlob()->Get();
	auto ssaoPSBlob = mShaders[L"SsaoPS"]->GetBlob()->Get();
	ssaoPsoDesc.VS = { reinterpret_cast<byte*>(ssaoVSBlob->GetBufferPointer()),ssaoVSBlob->GetBufferSize() };
	ssaoPsoDesc.PS = { reinterpret_cast<byte*>(ssaoPSBlob->GetBufferPointer()),ssaoPSBlob->GetBufferSize() };
	ssaoPsoDesc.InputLayout = { mNullInputLayout.data(),(uint32_t)mNullInputLayout.size() };
	ssaoPsoDesc.RTVFormats[0] = mSsao->GetAmbientMapFormat();
	ssaoPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	ssaoPsoDesc.DepthStencilState.DepthEnable = false;
	ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&ssaoPsoDesc, IID_PPV_ARGS(mPSOs[L"ssao"].GetAddressOf())));

	auto ssaoBlurPsoDesc = ssaoPsoDesc;
	auto ssaoBlurVSBlob = mShaders[L"SsaoBlurVS"]->GetBlob()->Get();
	auto ssaoBlurPSBlob = mShaders[L"SsaoBlurPS"]->GetBlob()->Get();
	ssaoBlurPsoDesc.VS = { reinterpret_cast<byte*>(ssaoBlurVSBlob->GetBufferPointer()),ssaoBlurVSBlob->GetBufferSize() };
	ssaoBlurPsoDesc.PS = { reinterpret_cast<byte*>(ssaoBlurPSBlob->GetBufferPointer()),ssaoBlurPSBlob->GetBufferSize() };
	ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&ssaoBlurPsoDesc, IID_PPV_ARGS(mPSOs[L"ssaoBlur"].GetAddressOf())));

	auto taaVelocityPsoDesc = basePsoDesc;
	auto taaVelocityVSBlob = mShaders[L"TaaVelocityVS"]->GetBlob()->Get();
	auto taaVelocityPSBlob = mShaders[L"TaaVelocityPS"]->GetBlob()->Get();
	taaVelocityPsoDesc.VS = { reinterpret_cast<byte*>(taaVelocityVSBlob->GetBufferPointer()),taaVelocityVSBlob->GetBufferSize() };
	taaVelocityPsoDesc.PS = { reinterpret_cast<byte*>(taaVelocityPSBlob->GetBufferPointer()),taaVelocityPSBlob->GetBufferSize() };
	taaVelocityPsoDesc.RTVFormats[0] = mTaa->GetVelocityFormat();
	ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&taaVelocityPsoDesc, IID_PPV_ARGS(mPSOs[L"taaVelocity"].GetAddressOf())));

	auto taaOutputPsoDesc = basePsoDesc;
	auto taaOutputVSBlob = mShaders[L"TaaOutputVS"]->GetBlob()->Get();
	auto taaOutputPSBlob = mShaders[L"TaaOutputPS"]->GetBlob()->Get();
	taaOutputPsoDesc.VS = { reinterpret_cast<byte*>(taaOutputVSBlob->GetBufferPointer()),taaOutputVSBlob->GetBufferSize() };
	taaOutputPsoDesc.PS = { reinterpret_cast<byte*>(taaOutputPSBlob->GetBufferPointer()),taaOutputPSBlob->GetBufferSize() };
	taaOutputPsoDesc.InputLayout = { mNullInputLayout.data(),(uint32_t)mNullInputLayout.size() };
	taaOutputPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	taaOutputPsoDesc.DepthStencilState.DepthEnable = false;
	ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&taaOutputPsoDesc, IID_PPV_ARGS(mPSOs[L"taaOutput"].GetAddressOf())));
}

void Carol::Renderer::Draw()
{
	ThrowIfFailed(mCurrFrameResource->FRCommandAllocator->Reset());
	ThrowIfFailed(mCommandList->Reset(mCurrFrameResource->FRCommandAllocator.Get(), nullptr));
	
	ID3D12DescriptorHeap* descriptorHeaps[] = {mCbvSrvUavHeap->Get()};
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
	mCommandList->SetGraphicsRootDescriptorTable(3, mTexturesSrv);
	mCommandList->SetGraphicsRootDescriptorTable(4, mMaterialBufferSrv);

	DrawNormalsAndDepth();
	mSsao->ComputeSsao(mCommandList.Get(), mSsaoRootSignature.Get(), mCurrFrameResource, 3);
	DrawTaa();
	
	mCommandList->Close();
	vector<ID3D12CommandList*> cmdLists{ mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(1, cmdLists.data());

	mDisplayManager->Present();
	mCurrFrameResource->Fence = ++mCurrFence;
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrFence));
}

void Carol::Renderer::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhWnd);
}

void Carol::Renderer::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void Carol::Renderer::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		mCamera->RotateY(dx);
		mCamera->Pitch(dy);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void Carol::Renderer::OnKeyboardInput()
{
	const float dt = mTimer->DeltaTime();

	if (GetAsyncKeyState('W') & 0x8000)
		mCamera->Walk(10.0f * dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera->Walk(-10.0f * dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera->Strafe(-10.0f * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera->Strafe(10.0f * dt);
}

void Carol::Renderer::DrawNormalsAndDepth()
{
	auto normalMap = mSsao->GetNormalMap();
	auto normalMapRtv = mSsao->GetNormalMapRtv();

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	normalMap->TransitionBarrier(mCommandList.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCommandList->ClearRenderTargetView(normalMapRtv, DirectX::Colors::Blue, 0, nullptr);
	mCommandList->ClearDepthStencilView(mDisplayManager->GetDepthStencilView(mDsvHeap.get()), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mCommandList->OMSetRenderTargets(1, &normalMapRtv, true, GetRvaluePtr(mDisplayManager->GetDepthStencilView(mDsvHeap.get())));

	mCommandList->SetGraphicsRootConstantBufferView(0, mCurrFrameResource->PassCB.GetVirtualAddress());
	mCommandList->SetPipelineState(mPSOs[L"drawNormals"].Get());
	DrawRenderItems(mOpaqueItems);

	normalMap->TransitionBarrier(mCommandList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
}

void Carol::Renderer::DrawTaa()
{
	DrawTaaCurrFrameMap();
	DrawTaaVelocityMap();
	DrawTaaOutput();
}

void Carol::Renderer::DrawTaaCurrFrameMap()
{
	auto currFrameMap = mTaa->GetCurrFrameMap();
	auto currFrameMapRtv = mTaa->GetCurrFrameMapRtv();

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	currFrameMap->TransitionBarrier(mCommandList.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCommandList->ClearRenderTargetView(currFrameMapRtv, DirectX::Colors::Gray, 0, nullptr);
	mCommandList->ClearDepthStencilView(mDisplayManager->GetDepthStencilView(mDsvHeap.get()), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mCommandList->OMSetRenderTargets(1, &currFrameMapRtv, true, GetRvaluePtr(mDisplayManager->GetDepthStencilView(mDsvHeap.get())));

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
	mCommandList->SetGraphicsRootConstantBufferView(0, mCurrFrameResource->PassCB.GetVirtualAddress());
	mCommandList->SetGraphicsRootDescriptorTable(3, mTexturesSrv);
	mCommandList->SetGraphicsRootDescriptorTable(4, mMaterialBufferSrv);
	mCommandList->SetGraphicsRootDescriptorTable(5, mSsao->GetSsaoMapSrv());

	mCommandList->SetPipelineState(mPSOs[L"opaque"].Get());
	DrawRenderItems(mOpaqueItems);
	currFrameMap->TransitionBarrier(mCommandList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
}

void Carol::Renderer::DrawTaaVelocityMap()
{
	auto velocityMap = mTaa->GetVelocityMap();
	auto velocityMapRtv = mTaa->GetVelocityMapRtv();

	velocityMap->TransitionBarrier(mCommandList.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCommandList->ClearRenderTargetView(velocityMapRtv, DirectX::Colors::Black, 0, nullptr);
	mCommandList->ClearDepthStencilView(mDisplayManager->GetDepthStencilView(mDsvHeap.get()), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mCommandList->OMSetRenderTargets(1, &velocityMapRtv, true, GetRvaluePtr(mDisplayManager->GetDepthStencilView(mDsvHeap.get())));

	mCommandList->SetPipelineState(mPSOs[L"taaVelocity"].Get());
	DrawRenderItems(mOpaqueItems);
	velocityMap->TransitionBarrier(mCommandList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
}

void Carol::Renderer::DrawTaaOutput()
{
	auto histMap = mTaa->GetHistFrameMap();
	auto histMapSrv = mTaa->GetHistFrameMapSrv();
	auto currMapSrv = mTaa->GetCurrFrameMapSrv();
	auto velocityMapSrv = mTaa->GetVelocityMapSrv();

	mDisplayManager->GetCurrBackBuffer()->TransitionBarrier(mCommandList.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	mCommandList.Get()->ClearRenderTargetView(mDisplayManager->GetCurrBackBufferView(mRtvHeap.get()), DirectX::Colors::Gray, 0, nullptr);
	mCommandList.Get()->OMSetRenderTargets(1, GetRvaluePtr(mDisplayManager->GetCurrBackBufferView(mRtvHeap.get())), true, nullptr);

	mCommandList.Get()->SetGraphicsRootDescriptorTable(3, mTaa->GetDepthSrv());

	mCommandList.Get()->SetPipelineState(mPSOs[L"taaOutput"].Get());
	mCommandList.Get()->IASetVertexBuffers(0, 0, nullptr);
	mCommandList.Get()->IASetIndexBuffer(nullptr);
	mCommandList.Get()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mCommandList.Get()->DrawInstanced(6, 1, 0, 0);

	histMap->TransitionBarrier(mCommandList.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST);
	mDisplayManager->GetCurrBackBuffer()->TransitionBarrier(mCommandList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
	mCommandList.Get()->CopyResource(histMap->Get(), mDisplayManager->GetCurrBackBuffer()->Get());
	histMap->TransitionBarrier(mCommandList.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
	mDisplayManager->GetCurrBackBuffer()->TransitionBarrier(mCommandList.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
}

void Carol::Renderer::DrawRenderItems(vector<RenderItem*>& ritems)
{
	for (int i=0;i<ritems.size();++i)
	{
		auto* ritem = ritems[i];

		mCommandList->IASetVertexBuffers(0, 1, GetRvaluePtr(ritem->Model->GetVertexBufferView()));
		mCommandList->IASetIndexBuffer(GetRvaluePtr(ritem->Model->GetIndexBufferView()));
		mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		auto objCBAddress = mCurrFrameResource->ObjCB.GetVirtualAddress() + ritem->ObjCBIndex * mCurrFrameResource->ObjCB.CalcConstantElementSize(sizeof(ObjectConstants));
		//auto skinnedCBAddress = mCurrFrameResource->SkinnedCB.GetVirtualAddress() + ritem->SkinnedCBIndex * mCurrFrameResource->SkinnedCB.CalcConstantElementSize(sizeof(SkinnedConstants));

		mCommandList->SetGraphicsRootConstantBufferView(1, objCBAddress);
		//mCommandList->SetGraphicsRootConstantBufferView(2, skinnedCBAddress);
		
		mCommandList->DrawIndexedInstanced(ritem->IndexCount, 1, ritem->StartIndexLocation, ritem->BaseVertexLocation, 0);
	}
}

void Carol::Renderer::Update()
{
	OnKeyboardInput();

	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFramesResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, LPCWSTR(nullptr), 0, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateCamera();
	UpdatePassCBs();
	UpdateObjCBs();
	//UpdateSkinnedCBs();
	UpdateSsaoCB();
}

void Carol::Renderer::UpdateCamera()
{
	mCamera->UpdateViewMatrix();
}

void Carol::Renderer::UpdatePassCBs()
{
	PassConstants passConstants;

	XMMATRIX view = mCamera->GetView();
	XMMATRIX invView = XMMatrixInverse(GetRvaluePtr(XMMatrixDeterminant(view)), view);
	
	XMFLOAT4X4 proj4x4f = mCamera->GetProj4x4f();
	mTaa->GetHalton(proj4x4f._31, proj4x4f._32, mClientWidth, mClientHeight);

	XMMATRIX proj = XMLoadFloat4x4(&proj4x4f);
	XMMATRIX invProj = XMMatrixInverse(GetRvaluePtr(XMMatrixDeterminant(proj)), proj);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX histViewProj = mTaa->GetHistViewProj();
	mTaa->SetHistViewProj(viewProj);
	XMMATRIX invViewProj = XMMatrixInverse(GetRvaluePtr(XMMatrixDeterminant(viewProj)), viewProj);

	XMMATRIX tex(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);
	XMMATRIX viewProjTex = XMMatrixMultiply(viewProj, tex);

	XMStoreFloat4x4(&passConstants.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&passConstants.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&passConstants.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&passConstants.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&passConstants.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&passConstants.HistViewProj, XMMatrixTranspose(histViewProj));
	XMStoreFloat4x4(&passConstants.InvViewProj, XMMatrixTranspose(invViewProj));
	XMStoreFloat4x4(&passConstants.ViewProjTex, XMMatrixTranspose(viewProjTex));

	passConstants.EyePosW = mCamera->GetPosition3f();
	passConstants.RenderTargetSize = { static_cast<float>(mClientWidth), static_cast<float>(mClientHeight) };
	passConstants.InvRenderTargetSize = { 1.0f / mClientWidth, 1.0f / mClientHeight };
	passConstants.NearZ = mCamera->GetNearZ();
	passConstants.FarZ = mCamera->GetFarZ();

	passConstants.Lights[0].Direction = { -1,-1,1 };

	mCurrFrameResource->PassCB.CopyData(0, passConstants);
}

void Carol::Renderer::UpdateObjCBs()
{
	for (auto& ritem : mRenderItems)
	{
		if (ritem->NumFramesDirty > 0)
		{
			ObjectConstants objectConstants;

			objectConstants.World = ritem->World;
			objectConstants.HistWorld = ritem->HistWorld;
			objectConstants.TexTransform = ritem->TexTransform;
			objectConstants.MatTBIndex = ritem->MatTBIndex;

			mCurrFrameResource->ObjCB.CopyData(ritem->ObjCBIndex, objectConstants);

			ritem->HistWorld = ritem->World;
			--ritem->NumFramesDirty;
		}
	}
}

void Carol::Renderer::UpdateSkinnedCBs()
{
	for (auto& skinnedInstancePair : mSkinnedInstances)
	{
		SkinnedConstants skinnedConstants;

		auto& skinnedInstance = skinnedInstancePair.second;
		if (skinnedInstance->SkinnedCBIndex == -1)
		{
			continue;
		}

		std::copy(std::begin(skinnedInstance->FinalTransforms),
			std::end(skinnedInstance->FinalTransforms), skinnedConstants.HistoryFinalTransforms);
		skinnedInstance->UpdateSkinnedModel(mTimer->DeltaTime());
		std::copy(std::begin(skinnedInstance->FinalTransforms),
			std::end(skinnedInstance->FinalTransforms), skinnedConstants.FinalTransforms);

		mCurrFrameResource->SkinnedCB.CopyData(skinnedInstance->SkinnedCBIndex, skinnedConstants);
	}
}

void Carol::Renderer::UpdateSsaoCB()
{
	SsaoConstants ssaoConstants;
	
	XMMATRIX proj = mCamera->GetProj();
	XMMATRIX tex(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMStoreFloat4x4(&ssaoConstants.Proj,XMMatrixTranspose(proj));
	XMStoreFloat4x4(&ssaoConstants.InvProj, XMMatrixTranspose(XMMatrixInverse(GetRvaluePtr(XMMatrixDeterminant(proj)), proj)));
	XMStoreFloat4x4(&ssaoConstants.ProjTex, XMMatrixTranspose(XMMatrixMultiply(proj, tex)));

	mSsao->GetOffsetVectors(ssaoConstants.OffsetVectors);
	auto blurWeights = mSsao->CalcGaussWeights(2.5f);
	ssaoConstants.BlurWeights[0] = XMFLOAT4(&blurWeights[0]);
	ssaoConstants.BlurWeights[1] = XMFLOAT4(&blurWeights[4]);
	ssaoConstants.BlurWeights[2] = XMFLOAT4(&blurWeights[8]);
	ssaoConstants.InvRenderTargetSize = { 1.0f / mClientWidth,1.0f / mClientHeight };

	mCurrFrameResource->SsaoCB.CopyData(0, ssaoConstants);
}

void Carol::Renderer::OnResize(uint32_t width, uint32_t height)
{
	BaseRenderer::OnResize(width, height);

	mSsao->OnResize(width,height);
	mSsao->RebuildDescriptors(mDisplayManager->GetDepthStencilBuffer());

	mTaa->OnResize(width,height);
	mTaa->RebuildDescriptors(mDisplayManager->GetDepthStencilBuffer());
}
