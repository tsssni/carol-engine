#include <render_pass/scene.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/descriptor.h>
#include <dx12/root_signature.h>
#include <dx12/indirect_command.h>
#include <scene/assimp.h>
#include <scene/timer.h>
#include <scene/camera.h>
#include <scene/texture.h>
#include <scene/scene_node.h>

namespace Carol
{
	using std::vector;
	using std::wstring;
	using std::unordered_map;
	using std::unique_ptr;
	using std::make_unique;
	using namespace DirectX;
	using Microsoft::WRL::ComPtr;
}

Carol::Scene::Scene(std::wstring name)
	:mRootNode(make_unique<SceneNode>()),
	mMeshes(MESH_TYPE_COUNT),
	mTexManager(make_unique<TextureManager>()),
	mIndirectCommandBuffer(gNumFrame),
	mMeshCB(gNumFrame),
	mSkinnedCB(gNumFrame),
	mInstanceFrustumCulledMarkBuffer(make_unique<RawBuffer>(
	2 << 16,
	gHeapManager->GetDefaultBuffersHeap(),
	D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
	D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)),
	mInstanceOcclusionPassedMarkBuffer(make_unique<RawBuffer>(
	2 << 16,
	gHeapManager->GetDefaultBuffersHeap(),
	D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
	D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
	
{
	mRootNode->Name = name;
	InitBuffers();
}

void Carol::Scene::DelayedDelete()
{
	mTexManager->DelayedDelete();
}

Carol::vector<Carol::wstring> Carol::Scene::GetAnimationClips(std::wstring modelName)
{
	return mModels[modelName]->GetAnimationClips();
}

std::vector<wstring> Carol::Scene::GetModelNames()
{
	vector<wstring> models;
	for (auto& modelMapPair : mModels)
	{
		models.push_back(modelMapPair.first);
	}

	return models;
}

bool Carol::Scene::IsAnyTransparentMeshes()
{
	return mMeshes[TRANSPARENT_STATIC].size() + mMeshes[TRANSPARENT_SKINNED].size();
}

void Carol::Scene::LoadModel(std::wstring name, std::wstring path, std::wstring textureDir, bool isSkinned)
{
	mRootNode->Children.push_back(make_unique<SceneNode>());
	auto& node = mRootNode->Children.back();
	node->Children.push_back(make_unique<SceneNode>());

	node->Name = name;
	mModels[name] = make_unique<AssimpModel>(
		node->Children[0].get(),
		path,
		textureDir,
		mTexManager.get(),
		isSkinned);

	for (auto& meshMapPair : mModels[name]->GetMeshes())
	{
		wstring meshName = name + L'_' + meshMapPair.first;
		auto& mesh = meshMapPair.second;

		uint32_t type = mesh->IsSkinned() | (mesh->IsTransparent() << 1);
		mMeshes[type][meshName] = mesh.get();
	}
}

void Carol::Scene::LoadGround()
{
	mModels[L"Ground"] = make_unique<Model>();
	mModels[L"Ground"]->LoadGround(mTexManager.get());
	
	mRootNode->Children.push_back(make_unique<SceneNode>());
	auto& node = mRootNode->Children.back();

	node->Name = L"Ground";
	node->Children.push_back(make_unique<SceneNode>());
	node->Children[0]->Meshes.push_back(mModels[L"Ground"]->GetMesh(L"Ground"));

	for (auto& meshMapPair : mModels[L"Ground"]->GetMeshes())
	{
		wstring meshName = L"Ground_" + meshMapPair.first;
		auto& mesh = meshMapPair.second;
		mMeshes[OPAQUE_STATIC][meshName] = mesh.get();
	}
}

void Carol::Scene::LoadSkyBox()
{
	mSkyBox = make_unique<Model>();
	mSkyBox->LoadSkyBox(mTexManager.get());
}

void Carol::Scene::UnloadModel(std::wstring modelName)
{
	for (auto itr = mRootNode->Children.begin(); itr != mRootNode->Children.end(); ++itr)
	{
		if ((*itr)->Name == modelName)
		{
			mRootNode->Children.erase(itr);
			break;
		}
	}

	for (auto& meshMapPair : mModels[modelName]->GetMeshes())
	{
		wstring meshName = modelName + L"_" + meshMapPair.first;
		auto& mesh = meshMapPair.second;

		uint32_t type = mesh->IsSkinned() | (mesh->IsTransparent() << 1);
		mMeshes[type].erase(meshName);
	}

	mModels.erase(modelName);
}

void Carol::Scene::ReleaseIntermediateBuffers()
{
	for (auto& modelMapPair : mModels)
	{
		auto& model = modelMapPair.second;
		model->ReleaseIntermediateBuffers();
	}
}

void Carol::Scene::ReleaseIntermediateBuffers(std::wstring modelName)
{
	mModels[modelName]->ReleaseIntermediateBuffers();
}

const Carol::unordered_map<Carol::wstring, Carol::Mesh*>& Carol::Scene::GetMeshes(MeshType type)
{
	return mMeshes[type];
}

uint32_t Carol::Scene::GetMeshesCount(MeshType type)
{
	return mMeshes[type].size();
}

const Carol::unordered_map<Carol::wstring, Carol::unique_ptr<Carol::Model>>& Carol::Scene::GetModels()
{
	return mModels;
}

uint32_t Carol::Scene::GetModelsCount()
{
	return mModels.size();
}

Carol::Mesh* Carol::Scene::GetSkyBox()
{
	return mSkyBox->GetMesh(L"SkyBox");
}

void Carol::Scene::SetWorld(std::wstring modelName, DirectX::XMMATRIX world)
{
	for (auto& node : mRootNode->Children)
	{
		if (node->Name == modelName)
		{
			XMStoreFloat4x4(&node->Transformation, world);
		}
	}
}

void Carol::Scene::SetAnimationClip(std::wstring modelName, std::wstring clipName)
{
	mModels[modelName]->SetAnimationClip(clipName);
}

void Carol::Scene::Contain(Camera* camera, std::vector<std::vector<Mesh*>>& meshes)
{
}

void Carol::Scene::ClearCullMark()
{
	static const uint32_t cullMarkClearValue = 0;
	gCommandList->ClearUnorderedAccessViewUint(mInstanceFrustumCulledMarkBuffer->GetGpuUav(), mInstanceFrustumCulledMarkBuffer->GetCpuUav(), mInstanceFrustumCulledMarkBuffer->Get(), &cullMarkClearValue, 0, nullptr);
	gCommandList->ClearUnorderedAccessViewUint(mInstanceOcclusionPassedMarkBuffer->GetGpuUav(), mInstanceOcclusionPassedMarkBuffer->GetCpuUav(), mInstanceOcclusionPassedMarkBuffer->Get(), &cullMarkClearValue, 0, nullptr);

	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		for (auto& meshMapPair : gScene->GetMeshes((MeshType)i))
		{
			auto& mesh = meshMapPair.second;
			mesh->ClearCullMark();
		}
	}
}

uint32_t Carol::Scene::GetMeshCBStartOffet(MeshType type)
{
	return mMeshStartOffset[type];
}

uint32_t Carol::Scene::GetMeshCBIdx()
{
	return mMeshCB[gCurrFrame]->GetGpuSrvIdx();
}

uint32_t Carol::Scene::GetCommandBufferIdx()
{
	return mIndirectCommandBuffer[gCurrFrame]->GetGpuSrvIdx();
}

uint32_t Carol::Scene::GetInstanceFrustumCulledMarkBufferIdx()
{
	return mInstanceFrustumCulledMarkBuffer->GetGpuUavIdx();
}

uint32_t Carol::Scene::GetInstanceOcclusionPassedMarkBufferIdx()
{
	return mInstanceOcclusionPassedMarkBuffer->GetGpuUavIdx();
}

void Carol::Scene::ExecuteIndirect(StructuredBuffer* indirectCmdBuffer)
{
	gCommandList->ExecuteIndirect(
		gCommandSignature.Get(),
		indirectCmdBuffer->GetNumElements(),
		indirectCmdBuffer->Get(),
		0,
		indirectCmdBuffer->Get(),
		indirectCmdBuffer->GetCounterOffset());

}

void Carol::Scene::DrawSkyBox(ID3D12PipelineState* skyBoxPSO)
{
	gCommandList->SetPipelineState(skyBoxPSO);
	gCommandList->SetGraphicsRootConstantBufferView(MESH_CB, gScene->GetSkyBox()->GetMeshCBAddress());
	static_cast<ID3D12GraphicsCommandList6*>(gCommandList.Get())->DispatchMesh(1, 1, 1);
}

void Carol::Scene::Update(Camera* camera, Timer* timer)
{
	for (auto& modelMapPair : mModels)
	{
		auto& model = modelMapPair.second;
		if (model->IsSkinned())
		{
			model->Update(timer);
		}
	}

	ProcessNode(mRootNode.get(), XMMatrixIdentity());

	uint32_t totalMeshCount = 0;

	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		totalMeshCount += GetMeshesCount(MeshType(i));

		if (i == 0)
		{
			mMeshStartOffset[i] = 0;
		}
		else
		{
			mMeshStartOffset[i] = mMeshStartOffset[i - 1] + GetMeshesCount(MeshType(i - 1));
		}
	}

	TestBufferSize(mIndirectCommandBuffer[gCurrFrame], totalMeshCount);
	TestBufferSize(mMeshCB[gCurrFrame], totalMeshCount);
	TestBufferSize(mSkinnedCB[gCurrFrame], gScene->GetModelsCount());

	int modelIdx = 0;
	for (auto& modelMapPair : gScene->GetModels())
	{
		auto& model = modelMapPair.second;

		if (model->IsSkinned())
		{
			mSkinnedCB[gCurrFrame]->CopyElements(model->GetSkinnedConstants(), modelIdx);
			model->SetSkinnedCBAddress(mSkinnedCB[gCurrFrame]->GetElementAddress(modelIdx));
			++modelIdx;
		}
	}

	int meshIdx = 0;
	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		for (auto& meshMapPair : gScene->GetMeshes(MeshType(i)))
		{
			auto& mesh = meshMapPair.second;
			mMeshCB[gCurrFrame]->CopyElements(mesh->GetMeshConstants(), meshIdx);
			mesh->SetMeshCBAddress(mMeshCB[gCurrFrame]->GetElementAddress(meshIdx));

			IndirectCommand indirectCmd;
			
			indirectCmd.MeshCBAddr = mesh->GetMeshCBAddress();
			indirectCmd.SkinnedCBAddr = mesh->GetSkinnedCBAddress();

			indirectCmd.DispatchMeshArgs.ThreadGroupCountX = ceilf(mesh->GetMeshletSize() * 1.f / WARP_SIZE);
			indirectCmd.DispatchMeshArgs.ThreadGroupCountY = 1;
			indirectCmd.DispatchMeshArgs.ThreadGroupCountZ = 1;

			mIndirectCommandBuffer[gCurrFrame]->CopyElements(&indirectCmd, meshIdx);

			++meshIdx;
		}
	}

	mMeshCB[gCurrFrame]->CopyElements(gScene->GetSkyBox()->GetMeshConstants(), meshIdx);
	gScene->GetSkyBox()->SetMeshCBAddress(mMeshCB[gCurrFrame]->GetElementAddress(meshIdx));

}

void Carol::Scene::ProcessNode(SceneNode* node, DirectX::XMMATRIX parentToRoot)
{
	XMMATRIX toParent = XMLoadFloat4x4(&node->Transformation);
	XMMATRIX world = toParent * parentToRoot;

	for(auto& mesh : node->Meshes)
	{
		mesh->Update(world);
	}

	for (auto& child : node->Children)
	{
		ProcessNode(child.get(), world);
	}
}

void Carol::Scene::InitBuffers()
{
	for (int i = 0; i < gNumFrame; ++i)
	{
		ResizeBuffer(mIndirectCommandBuffer[i], 1024, sizeof(IndirectCommand), false);
		ResizeBuffer(mMeshCB[i], 1024, sizeof(MeshConstants), true);
		ResizeBuffer(mSkinnedCB[i], 1024, sizeof(SkinnedConstants), true);
	}
}

void Carol::Scene::TestBufferSize(std::unique_ptr<StructuredBuffer>& buffer, uint32_t numElements)
{
	if (buffer->GetNumElements() < numElements)
	{
		ResizeBuffer(buffer, numElements, buffer->GetElementSize(), buffer->IsConstant());
	}
}

void Carol::Scene::ResizeBuffer(std::unique_ptr<StructuredBuffer>& buffer, uint32_t numElements, uint32_t elementSize, bool isConstant)
{
	buffer = make_unique<StructuredBuffer>(
		numElements,
		elementSize,
		gHeapManager->GetUploadBuffersHeap(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_NONE,
		isConstant);
}
