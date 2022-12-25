#include <render_pass/mesh.h>
#include <render_pass/global_resources.h>
#include <render_pass/shadow.h>
#include <scene/assimp.h>
#include <scene/skinned_data.h>
#include <scene/texture.h>
#include <scene/light.h>
#include <scene/camera.h>
#include <scene/timer.h>
#include <dx12/heap.h>
#include <dx12/descriptor_allocator.h>
#include <dx12/root_signature.h>

namespace Carol {
	using std::vector;
	using std::wstring;
	using std::unique_ptr;
	using std::make_unique;
	using namespace DirectX;
}

Carol::MeshData::MeshData(
	GlobalResources* globalResources,
	unique_ptr<Mesh>& mesh,
	HeapAllocInfo* skinnedInfo,
	D3D12_VERTEX_BUFFER_VIEW* vbv,
	D3D12_INDEX_BUFFER_VIEW* ibv)
	:mGlobalResources(globalResources),
	mMesh(std::move(mesh)),
	mMeshConstants(make_unique<MeshConstants>()),
	mMeshCBAllocInfo(make_unique<HeapAllocInfo>()),
	mSkinnedCBAllocInfo(skinnedInfo),
	mVbv(vbv),
	mIbv(ibv)
{
	Material mat = mMesh->GetMaterial();
	mMeshConstants->FresnelR0 = mat.FresnelR0;
	mMeshConstants->Roughness = mat.Roughness;
	XMStoreFloat4x4(&mMeshConstants->World, XMMatrixIdentity());
	XMStoreFloat4x4(&mMeshConstants->HistWorld, XMMatrixIdentity());
	mGlobalResources->TexManager->LoadTexture(mMesh->GetDiffuseMapPath());
	mGlobalResources->TexManager->LoadTexture(mMesh->GetNormalMapPath());
}

Carol::MeshData::~MeshData()
{
	mGlobalResources->TexManager->UnloadTexture(mMesh->GetDiffuseMapPath());
	mGlobalResources->TexManager->UnloadTexture(mMesh->GetNormalMapPath());
}

void Carol::MeshData::ReleaseIntermediateBuffers()
{
	mGlobalResources->TexManager->ReleaseIntermediateBuffers(mMesh->GetDiffuseMapPath());
	mGlobalResources->TexManager->ReleaseIntermediateBuffers(mMesh->GetNormalMapPath());
}

void Carol::MeshData::UpdateConstants()
{
	mMeshConstants->MeshDiffuseMapIdx = mGlobalResources->TexManager->CollectGpuTextures(mMesh->GetDiffuseMapPath());
	mMeshConstants->MeshNormalMapIdx = mGlobalResources->TexManager->CollectGpuTextures(mMesh->GetNormalMapPath());
	mMeshConstants->HistWorld = mMeshConstants->World;

	MeshesPass::MeshCBHeap->DeleteResource(mMeshCBAllocInfo.get());
	MeshesPass::MeshCBHeap->CreateResource(mMeshCBAllocInfo.get());
	MeshesPass::MeshCBHeap->CopyData(mMeshCBAllocInfo.get(), mMeshConstants.get());
}

bool Carol::MeshData::IsTransparent()
{
	return mMesh->IsTransparent();
}

void Carol::MeshData::SetWorld(XMMATRIX world)
{
	mMeshConstants->HistWorld = mMeshConstants->World;
	XMStoreFloat4x4(&mMeshConstants->World, world);
}

Carol::BoundingBox Carol::MeshData::GetBoundingBox()
{
	return mMesh->GetBoundingBox();
}

D3D12_VERTEX_BUFFER_VIEW* Carol::MeshData::GetVertexBufferView()
{
	return mVbv;
}

D3D12_INDEX_BUFFER_VIEW* Carol::MeshData::GetIndexBufferView()
{
	return mIbv;
}

uint32_t Carol::MeshData::GetBaseVertexLocation()
{
	return mMesh->GetBaseVertexLocation();
}

uint32_t Carol::MeshData::GetStartIndexLocation()
{
	return mMesh->GetStartIndexLocation();
}

uint32_t Carol::MeshData::GetIndexCount()
{
	return mMesh->GetIndexCount();
}

Carol::HeapAllocInfo* Carol::MeshData::GetMeshCBAllocInfo()
{
	return mMeshCBAllocInfo.get();
}

Carol::HeapAllocInfo* Carol::MeshData::GetSkinnedCBAllocInfo()
{
	return mSkinnedCBAllocInfo;
}

Carol::ModelData::ModelData(GlobalResources* globalResources, const wstring& modelPath, const wstring& texDir, bool isSkinned)
	:mGlobalResources(globalResources),
	mModel(make_unique<AssimpModel>(
		globalResources->CommandList,
		globalResources->DefaultBuffersHeap,
		globalResources->UploadBuffersHeap,
		modelPath,
		texDir,
		isSkinned)),
	mSkinnedConstants(make_unique<SkinnedConstants>()),
	mSkinnedCBAllocInfo(make_unique<HeapAllocInfo>())
{
	auto& meshMap = mModel->GetMeshes();
	for (auto& meshMapPair : meshMap)
	{
		auto& meshName = meshMapPair.first;
		auto& mesh = meshMapPair.second;

		mMeshes[meshName] = make_unique<MeshData>(
				mGlobalResources,
				const_cast<unique_ptr<Mesh>&>(mesh),
				mSkinnedCBAllocInfo.get(),
				mModel->GetVertexBufferView(),
				mModel->GetIndexBufferView());
	}

	SetWorld(XMMatrixIdentity());
}

Carol::ModelData::ModelData(GlobalResources* globalResources)
	:mGlobalResources(globalResources)
{
}

void Carol::ModelData::LoadGround()
{
	mModel = make_unique<Model>();
	mModel->LoadGround(
		mGlobalResources->CommandList,
		mGlobalResources->DefaultBuffersHeap,
		mGlobalResources->UploadBuffersHeap);

	auto& meshMap = mModel->GetMeshes();
	for (auto& meshMapPair : meshMap)
	{
		auto& meshName = meshMapPair.first;
		auto& mesh = meshMapPair.second;

		mMeshes[meshName] = make_unique<MeshData>(
				mGlobalResources,
				const_cast<unique_ptr<Mesh>&>(mesh),
				mSkinnedCBAllocInfo.get(),
				mModel->GetVertexBufferView(),
				mModel->GetIndexBufferView());
	}

	SetWorld(XMMatrixIdentity());
}

void Carol::ModelData::LoadSkyBox()
{
	mModel = make_unique<Model>();
	mModel->LoadSkyBox(
		mGlobalResources->CommandList,
		mGlobalResources->DefaultBuffersHeap,
		mGlobalResources->UploadBuffersHeap);

	auto& meshMap = mModel->GetMeshes();
	for (auto& meshMapPair : meshMap)
	{
		auto& meshName = meshMapPair.first;
		auto& mesh = meshMapPair.second;

		mMeshes[meshName] = make_unique<MeshData>(
				mGlobalResources,
				const_cast<unique_ptr<Mesh>&>(mesh),
				mSkinnedCBAllocInfo.get(),
				mModel->GetVertexBufferView(),
				mModel->GetIndexBufferView());
	}

	SetWorld(XMMatrixIdentity());
}

bool Carol::ModelData::IsSkinned()
{
	return mModel->IsSkinned();
}

void Carol::ModelData::SetAnimationClip(std::wstring clipName)
{
	mAnimationClip = mModel->GetAnimationClip(clipName);
}

Carol::vector<Carol::wstring> Carol::ModelData::GetAnimationClips()
{
	return mModel->GetAnimationClips();
}

Carol::vector<Carol::MeshData*> Carol::ModelData::GetMeshes()
{
	vector<MeshData*> meshes;
	for (auto& meshMapPair : mMeshes)
	{
		meshes.push_back(meshMapPair.second.get());
	}

	return meshes;
}

void Carol::ModelData::ReleaseIntermediateBuffer()
{
	mModel->ReleaseIntermediateBuffers();
	for (auto* mesh : GetMeshes())
	{
		mesh->ReleaseIntermediateBuffers();
	}
}

void Carol::ModelData::UpdateAnimationClip()
{
	mTimePos += mGlobalResources->Timer->DeltaTime();

	if (mTimePos > mAnimationClip->GetClipEndTime())
	{
		mTimePos = 0;
	}
	
	auto& boneHierarchy = mModel->GetBoneHierarchy();
	auto& boneOffsets = mModel->GetBoneOffsets();
	uint32_t boneCount = boneHierarchy.size();

	vector<XMFLOAT4X4> toParentTransforms(boneCount);
	vector<XMFLOAT4X4> toRootTransforms(boneCount);
	vector<XMFLOAT4X4> finalTransforms(boneCount);
	mAnimationClip->Interpolate(mTimePos, toParentTransforms);

	for (int i = 0; i < boneCount; ++i)
	{
		XMMATRIX toParent = XMLoadFloat4x4(&toParentTransforms[i]);
		XMMATRIX parentToRoot = boneHierarchy[i] != -1 ? XMLoadFloat4x4(&toRootTransforms[boneHierarchy[i]]) : XMMatrixIdentity();

		XMMATRIX toRoot = toParent * parentToRoot;
		XMStoreFloat4x4(&toRootTransforms[i], toRoot);
	}

	for (int i = 0; i < boneCount; ++i)
	{
		XMMATRIX offset = XMLoadFloat4x4(&boneOffsets[i]);
		XMMATRIX toRoot = XMLoadFloat4x4(&toRootTransforms[i]);

		XMStoreFloat4x4(&finalTransforms[i], XMMatrixTranspose(offset * toRoot));
	}

	std::copy(std::begin(mSkinnedConstants->FinalTransforms), std::end(mSkinnedConstants->FinalTransforms), mSkinnedConstants->HistoryFinalTransforms);
	std::copy(std::begin(finalTransforms), std::end(finalTransforms), mSkinnedConstants->FinalTransforms);

	MeshesPass::SkinnedCBHeap->DeleteResource(mSkinnedCBAllocInfo.get());
	MeshesPass::SkinnedCBHeap->CreateResource(mSkinnedCBAllocInfo.get());
	MeshesPass::SkinnedCBHeap->CopyData(mSkinnedCBAllocInfo.get(), mSkinnedConstants.get());
}

void Carol::ModelData::SetWorld(Carol::XMMATRIX world)
{
	for (auto& meshMapPair : mMeshes)
	{
		auto* mesh = meshMapPair.second.get();
		mesh->SetWorld(world);
	}
}

void Carol::MeshesPass::LoadModel(const std::wstring& modelName, const std::wstring& path, const std::wstring texDir, bool isSkinned)	
{
	mModels[modelName] = make_unique<ModelData>(mGlobalResources, path, texDir, isSkinned);
}

void Carol::MeshesPass::SetWorld(const std::wstring& modelName, DirectX::XMMATRIX world)
{
	mModels[modelName]->SetWorld(world);
}

void Carol::MeshesPass::LoadGround()
{
	mModels[L"Ground"] = make_unique<ModelData>(mGlobalResources);
	mModels[L"Ground"]->LoadGround();
}

void Carol::MeshesPass::LoadSkyBox()
{
	mSkyBox = make_unique<ModelData>(mGlobalResources);
	mSkyBox->LoadSkyBox();
}

void Carol::MeshesPass::UnloadModel(const std::wstring& modelName)
{
	if (mModels.count(modelName))
	{
		mModels.erase(modelName);
	}
}

Carol::vector<Carol::wstring> Carol::MeshesPass::GetModels()
{
	vector<wstring> models;
	for (auto& modelMapPair : mModels)
	{
		models.push_back(modelMapPair.first);
	}

	return models;
}

void Carol::MeshesPass::SetAnimationClip(const wstring& modelName, const wstring& clipName)
{
	mModels[modelName]->SetAnimationClip(clipName);
}

Carol::MeshesPass::MeshesPass(GlobalResources* globalResources)
	:RenderPass(globalResources)
{
}

void Carol::MeshesPass::Update()
{
	mMainCameraContainedOpaqueStaticMeshes.clear();
	mMainCameraContainedOpaqueSkinnedMeshes.clear();
	mMainCameraContainedTransparentStaticMeshes.clear();
	mMainCameraContainedTransparentSkinnedMeshes.clear();

	for (auto& modelMapPair : mModels)
	{
		auto& model = modelMapPair.second;
		bool isSkinned = model->IsSkinned();

		if (isSkinned)
		{
			model->UpdateAnimationClip();
		}		
	}

	GetContainedMeshes(
		mGlobalResources->Camera,
		mMainCameraContainedOpaqueStaticMeshes,
		mMainCameraContainedOpaqueSkinnedMeshes,
		mMainCameraContainedTransparentStaticMeshes,
		mMainCameraContainedTransparentSkinnedMeshes);

	UpdateMeshes();
}

Carol::vector<Carol::wstring> Carol::MeshesPass::GetAnimationClips(std::wstring modelName)
{
	return mModels[modelName]->GetAnimationClips();
}

uint32_t Carol::MeshesPass::NumTransparentMeshes()
{
	return mMainCameraContainedTransparentStaticMeshes.size()+mMainCameraContainedTransparentSkinnedMeshes.size();
}

void Carol::MeshesPass::CopyDescriptors()
{
	RenderPass::CopyDescriptors();
}

void Carol::MeshesPass::InitShaders()
{
}

void Carol::MeshesPass::InitPSOs()
{
}

void Carol::MeshesPass::Draw()
{
	for (auto& modelMapPair : mModels)
	{
		auto& model = modelMapPair.second;

		for (auto& mesh : model->GetMeshes())
		{
			Draw(mesh);
		}
	}
}

void Carol::MeshesPass::Draw(MeshData* mesh)
{
	mGlobalResources->CommandList->IASetVertexBuffers(0, 1, mesh->GetVertexBufferView());
	mGlobalResources->CommandList->IASetIndexBuffer(mesh->GetIndexBufferView());
	mGlobalResources->CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mGlobalResources->CommandList->SetGraphicsRootConstantBufferView(RootSignature::ROOT_SIGNATURE_MESH_CB, MeshCBHeap->GetGPUVirtualAddress(mesh->GetMeshCBAllocInfo()));
	mGlobalResources->CommandList->SetGraphicsRootConstantBufferView(RootSignature::ROOT_SIGNATURE_SKINNED_CB, SkinnedCBHeap->GetGPUVirtualAddress(mesh->GetSkinnedCBAllocInfo()));

	mGlobalResources->CommandList->DrawIndexedInstanced(
		mesh->GetIndexCount(),
		1,
		mesh->GetStartIndexLocation(),
		mesh->GetBaseVertexLocation(),
		0
	);
}

void Carol::MeshesPass::OnResize()
{
}

void Carol::MeshesPass::ReleaseIntermediateBuffers()
{
	for (auto& modelMapPair : mModels)
	{
		auto& model = modelMapPair.second;
		model->ReleaseIntermediateBuffer();
	}
}

void Carol::MeshesPass::ReleaseIntermediateBuffers(const std::wstring& modelName)
{
	mModels[modelName]->ReleaseIntermediateBuffer();
}

void Carol::MeshesPass::GetContainedMeshes(Camera* camera, std::vector<MeshData*>& opaqueStaticMeshes, std::vector<MeshData*>& opaqueSkinnedMeshes, std::vector<MeshData*>& transparentStaticMeshes, std::vector<MeshData*>& transparentSkinnedMeshes)
{
	for (auto& modelMapPair : mModels)
	{
		auto& model = modelMapPair.second;
		bool isSkinned = model->IsSkinned();

		for (auto& mesh : model->GetMeshes())
		{
			if (camera->Contains(mesh->GetBoundingBox()))
			{
				if (!isSkinned && !mesh->IsTransparent())
				{
					opaqueStaticMeshes.push_back(mesh);
				}
				else if (isSkinned && !mesh->IsTransparent())
				{
					opaqueSkinnedMeshes.push_back(mesh);
				}
				else if (!isSkinned && mesh->IsTransparent())
				{
					transparentStaticMeshes.push_back(mesh);
				}
				else
				{
					transparentSkinnedMeshes.push_back(mesh);
				}
			}
		}
		
	}
}

void Carol::MeshesPass::UpdateMeshes()
{
	for (auto* mesh : mMainCameraContainedOpaqueStaticMeshes)
	{
		mesh->UpdateConstants();
	}

	for (auto* mesh : mMainCameraContainedOpaqueSkinnedMeshes)
	{
		mesh->UpdateConstants();
	}

	for (auto* mesh : mMainCameraContainedTransparentStaticMeshes)
	{
		mesh->UpdateConstants();
	}

	for (auto* mesh : mMainCameraContainedTransparentSkinnedMeshes)
	{
		mesh->UpdateConstants();
	}

	auto* skyBoxMesh = mSkyBox->GetMeshes()[0];
	skyBoxMesh->UpdateConstants();
}

void Carol::MeshesPass::UpdateMesh(MeshData* mesh)
{
	mesh->UpdateConstants();
}

void Carol::MeshesPass::DrawContainedMeshes(Camera* camera, ID3D12PipelineState* opaqueStaticPSO, ID3D12PipelineState* opaqueSkinnedPSO, ID3D12PipelineState* transparentStaticPSO, ID3D12PipelineState* transparentSkinnedPSO)
{
	vector<MeshData*> opaqueStaticMeshes;
	vector<MeshData*> opaqueSkinnedMeshes;
	vector<MeshData*> transparentStaticMeshes;
	vector<MeshData*> transparentSkinnedMeshes;

	GetContainedMeshes(
		camera,
		opaqueStaticMeshes,
		opaqueSkinnedMeshes,
		transparentStaticMeshes,
		transparentSkinnedMeshes
	);
	
	mGlobalResources->CommandList->SetPipelineState(opaqueStaticPSO);
	for (auto* mesh : opaqueStaticMeshes)
	{
		Draw(mesh);
	}

	mGlobalResources->CommandList->SetPipelineState(opaqueSkinnedPSO);
	for (auto* mesh : opaqueSkinnedMeshes)
	{
		Draw(mesh);
	}

	mGlobalResources->CommandList->SetPipelineState(transparentStaticPSO);
	for (auto* mesh : transparentStaticMeshes)
	{
		Draw(mesh);
	}

	mGlobalResources->CommandList->SetPipelineState(transparentSkinnedPSO);
	for (auto* mesh : transparentSkinnedMeshes)
	{
		Draw(mesh);
	}
}

void Carol::MeshesPass::DrawContainedOpaqueMeshes(Camera* camera, ID3D12PipelineState* opaqueStaticPSO, ID3D12PipelineState* opaqueSkinnedPSO)
{
	vector<MeshData*> opaqueStaticMeshes;
	vector<MeshData*> opaqueSkinnedMeshes;
	vector<MeshData*> transparentStaticMeshes;
	vector<MeshData*> transparentSkinnedMeshes;

	GetContainedMeshes(
		camera,
		opaqueStaticMeshes,
		opaqueSkinnedMeshes,
		transparentStaticMeshes,
		transparentSkinnedMeshes
	);
	
	mGlobalResources->CommandList->SetPipelineState(opaqueStaticPSO);
	for (auto* mesh : opaqueStaticMeshes)
	{
		Draw(mesh);
	}

	mGlobalResources->CommandList->SetPipelineState(opaqueSkinnedPSO);
	for (auto* mesh : opaqueSkinnedMeshes)
	{
		Draw(mesh);
	}
}

void Carol::MeshesPass::DrawContainedTransparentMeshes(Camera* camera, ID3D12PipelineState* transparentStaticPSO, ID3D12PipelineState* transparentSkinnedPSO)
{
	vector<MeshData*> opaqueStaticMeshes;
	vector<MeshData*> opaqueSkinnedMeshes;
	vector<MeshData*> transparentStaticMeshes;
	vector<MeshData*> transparentSkinnedMeshes;

	GetContainedMeshes(
		camera,
		opaqueStaticMeshes,
		opaqueSkinnedMeshes,
		transparentStaticMeshes,
		transparentSkinnedMeshes
	);
	
	mGlobalResources->CommandList->SetPipelineState(transparentStaticPSO);
	for (auto* mesh : transparentStaticMeshes)
	{
		Draw(mesh);
	}

	mGlobalResources->CommandList->SetPipelineState(transparentSkinnedPSO);
	for (auto* mesh : transparentSkinnedMeshes)
	{
		Draw(mesh);
	}
}

void Carol::MeshesPass::DrawMainCameraContainedMeshes(ID3D12PipelineState* opaqueStaticPSO, ID3D12PipelineState* opaqueSkinnedPSO, ID3D12PipelineState* transparentStaticPSO, ID3D12PipelineState* transparentSkinnedPSO)
{
	mGlobalResources->CommandList->SetPipelineState(opaqueStaticPSO);
	for (auto* mesh : mMainCameraContainedOpaqueStaticMeshes)
	{
		Draw(mesh);
	}

	mGlobalResources->CommandList->SetPipelineState(opaqueSkinnedPSO);
	for (auto* mesh : mMainCameraContainedOpaqueSkinnedMeshes)
	{
		Draw(mesh);
	}

	mGlobalResources->CommandList->SetPipelineState(transparentStaticPSO);
	for (auto* mesh : mMainCameraContainedTransparentStaticMeshes)
	{
		Draw(mesh);
	}

	mGlobalResources->CommandList->SetPipelineState(transparentSkinnedPSO);
	for (auto* mesh : mMainCameraContainedTransparentSkinnedMeshes)
	{
		Draw(mesh);
	}
}

void Carol::MeshesPass::DrawMainCameraContainedOpaqueMeshes(ID3D12PipelineState* opaqueStaticPSO, ID3D12PipelineState* opaqueSkinnedPSO)
{
	mGlobalResources->CommandList->SetPipelineState(opaqueStaticPSO);
	for (auto* mesh : mMainCameraContainedOpaqueStaticMeshes)
	{
		Draw(mesh);
	}

	mGlobalResources->CommandList->SetPipelineState(opaqueSkinnedPSO);
	for (auto* mesh : mMainCameraContainedOpaqueSkinnedMeshes)
	{
		Draw(mesh);
	}
}

void Carol::MeshesPass::DrawMainCameraContainedTransparentMeshes(ID3D12PipelineState* transparentStaticPSO, ID3D12PipelineState* transparentSkinnedPSO)
{
	mGlobalResources->CommandList->SetPipelineState(transparentStaticPSO);
	for (auto* mesh : mMainCameraContainedTransparentStaticMeshes)
	{
		Draw(mesh);
	}

	mGlobalResources->CommandList->SetPipelineState(transparentSkinnedPSO);
	for (auto* mesh : mMainCameraContainedTransparentSkinnedMeshes)
	{
		Draw(mesh);
	}
}

void Carol::MeshesPass::DrawSkyBox(ID3D12PipelineState* skyBoxPSO)
{
	mGlobalResources->CommandList->SetPipelineState(skyBoxPSO);
	Draw(mSkyBox->GetMeshes()[0]);
}

void Carol::MeshesPass::InitMeshCBHeap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
	if (!MeshCBHeap)
	{
		MeshCBHeap = make_unique<CircularHeap>(device, cmdList, true, 2048, sizeof(MeshConstants));
	}
}

void Carol::MeshesPass::InitSkinnedCBHeap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
	if (!SkinnedCBHeap)
	{
		SkinnedCBHeap = make_unique<CircularHeap>(device, cmdList, true, 32, sizeof(SkinnedConstants));
	}
}

void Carol::MeshesPass::InitResources()
{
}

void Carol::MeshesPass::InitDescriptors()
{
}
