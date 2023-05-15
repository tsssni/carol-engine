#include <scene/scene.h>
#include <dx12/heap.h>
#include <dx12/resource.h>
#include <dx12/indirect_command.h>
#include <scene/scene_node.h>
#include <scene/assimp.h>
#include <global.h>

namespace Carol
{
	using std::vector;
	using std::string;
	using std::string_view;
	using std::unordered_map;
	using std::unique_ptr;
	using std::make_unique;
	using namespace DirectX;
	using Microsoft::WRL::ComPtr;
}

Carol::SceneManager::SceneManager(
	string_view name)
	:mRootNode(make_unique<SceneNode>()),
	mMeshes(MESH_TYPE_COUNT)
{
	mRootNode->Name = name;
	InitBuffers();
}

void Carol::SceneManager::InitBuffers()
{
	mInstanceFrustumCulledMarkBuffer = make_unique<RawBuffer>(
		2 << 16,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mInstanceOcclusionCulledMarkBuffer = make_unique<RawBuffer>(
		2 << 16,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mInstanceCulledMarkBuffer = make_unique<RawBuffer>(
		2 << 16,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mIndirectCommandBufferPool = make_unique<StructuredBufferPool>(
		1024,
		sizeof(IndirectCommand),
		gHeapManager->GetUploadBuffersHeap(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_NONE,
		false);

	mMeshBufferPool = make_unique<StructuredBufferPool>(
		1024,
		sizeof(MeshConstants),
		gHeapManager->GetUploadBuffersHeap(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_NONE,
		true);

	mSkinnedBufferPool = make_unique<StructuredBufferPool>(
		1024,
		sizeof(SkinnedConstants),
		gHeapManager->GetUploadBuffersHeap(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_NONE,
		true);
}

Carol::vector<Carol::string_view> Carol::SceneManager::GetAnimationClips(string_view modelName)const
{
	return mModels.at(modelName.data())->GetAnimationClips();
}

Carol::vector<Carol::string_view> Carol::SceneManager::GetModelNames()const
{
	vector<string_view> models;
	for (auto& [name, model] : mModels)
	{
		models.push_back(string_view(name.c_str(), name.size()));
	}

	return models;
}

bool Carol::SceneManager::IsAnyOpaqueMeshes() const
{
	return mMeshes[OPAQUE_STATIC].size() + mMeshes[OPAQUE_SKINNED].size();
}

bool Carol::SceneManager::IsAnyTransparentMeshes()const
{
	return mMeshes[TRANSPARENT_STATIC].size() + mMeshes[TRANSPARENT_SKINNED].size();
}

void Carol::SceneManager::LoadModel(
	string_view name,
	string_view path,
	string_view textureDir,
	bool isSkinned)
{
	mRootNode->Children.push_back(make_unique<SceneNode>());
	auto& node = mRootNode->Children.back();
	node->Children.push_back(make_unique<SceneNode>());

	node->Name = name;
	mModels[node->Name] = make_unique<AssimpModel>(
		node.get(),
		path,
		textureDir,
		isSkinned);

	for (auto& [name, mesh] : mModels[node->Name]->GetMeshes())
	{
		string meshName = node->Name + '_' + name;
		uint32_t type = uint32_t(mesh->IsSkinned()) | (uint32_t(mesh->IsTransparent()) << 1);
		mMeshes[type][meshName] = mesh.get();
	}
}

void Carol::SceneManager::UnloadModel(string_view modelName)
{
	for (auto itr = mRootNode->Children.begin(); itr != mRootNode->Children.end(); ++itr)
	{
		if ((*itr)->Name == modelName)
		{
			mRootNode->Children.erase(itr);
			break;
		}
	}

	string name(modelName);

	for (auto& [meshName, mesh] : mModels[name]->GetMeshes())
	{
		string modelMeshName = name + "_" + meshName;
		uint32_t type = uint32_t(mesh->IsSkinned()) | (uint32_t(mesh->IsTransparent()) << 1);
		mMeshes[type].erase(modelMeshName);
	}

	mModels.erase(name);
}

void Carol::SceneManager::ReleaseIntermediateBuffers()
{
	for (auto& [name ,model] : mModels)
	{
		model->ReleaseIntermediateBuffers();
	}
}

void Carol::SceneManager::ReleaseIntermediateBuffers(string_view modelName)
{
	mModels[modelName.data()]->ReleaseIntermediateBuffers();
}

uint32_t Carol::SceneManager::GetMeshesCount(MeshType type)const
{
	return mMeshes[type].size();
}

uint32_t Carol::SceneManager::GetModelsCount()const
{
	return mModels.size();
}

void Carol::SceneManager::SetWorld(string_view modelName, DirectX::XMMATRIX world)
{
	for (auto& node : mRootNode->Children)
	{
		if (node->Name == modelName)
		{
			XMStoreFloat4x4(&node->Transformation, world);
		}
	}
}

void Carol::SceneManager::SetAnimationClip(string_view modelName, string_view clipName)
{
	mModels[modelName.data()]->SetAnimationClip(clipName);
}

void Carol::SceneManager::Contain(Camera* camera, std::vector<std::vector<Mesh*>>& meshes)
{
}

void Carol::SceneManager::ClearCullMark()
{
	static const uint32_t clear0 = 0;
	static const uint32_t clear1 = 0xffffffff;
	gGraphicsCommandList->ClearUnorderedAccessViewUint(mInstanceFrustumCulledMarkBuffer->GetGpuUav(), mInstanceFrustumCulledMarkBuffer->GetCpuUav(), mInstanceFrustumCulledMarkBuffer->Get(), &clear0, 0, nullptr);
	gGraphicsCommandList->ClearUnorderedAccessViewUint(mInstanceOcclusionCulledMarkBuffer->GetGpuUav(), mInstanceOcclusionCulledMarkBuffer->GetCpuUav(), mInstanceOcclusionCulledMarkBuffer->Get(), &clear1, 0, nullptr);
	gGraphicsCommandList->ClearUnorderedAccessViewUint(mInstanceCulledMarkBuffer->GetGpuUav(), mInstanceCulledMarkBuffer->GetCpuUav(), mInstanceCulledMarkBuffer->Get(), &clear1, 0, nullptr);

	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		for (auto& [name, mesh] : mMeshes[i])
		{
			mesh->ClearCullMark();
		}
	}
}

uint32_t Carol::SceneManager::GetMeshCBStartOffet(MeshType type)const
{
	return mMeshStartOffset[type];
}

uint32_t Carol::SceneManager::GetMeshBufferIdx()const
{
	return mMeshBuffer->GetGpuSrvIdx();
}

uint32_t Carol::SceneManager::GetCommandBufferIdx()const
{
	return mIndirectCommandBuffer->GetGpuSrvIdx();
}

uint32_t Carol::SceneManager::GetInstanceFrustumCulledMarkBufferIdx()const
{
	return mInstanceFrustumCulledMarkBuffer->GetGpuUavIdx();
}

uint32_t Carol::SceneManager::GetInstanceOcclusionCulledMarkBufferIdx()const
{
	return mInstanceOcclusionCulledMarkBuffer->GetGpuUavIdx();
}

uint32_t Carol::SceneManager::GetInstanceCulledMarkBufferIdx() const
{
	return mInstanceCulledMarkBuffer->GetGpuUavIdx();
}

void Carol::SceneManager::Update(Timer* timer, uint64_t cpuFenceValue, uint64_t completedFenceValue)
{
	for (auto& [name, model] : mModels)
	{
		model->Update(timer);
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
	
	mIndirectCommandBufferPool->DiscardBuffer(mIndirectCommandBuffer.release(), cpuFenceValue);
	mMeshBufferPool->DiscardBuffer(mMeshBuffer.release(), cpuFenceValue);
	mSkinnedBufferPool->DiscardBuffer(mSkinnedBuffer.release(), cpuFenceValue);
	
	mIndirectCommandBuffer = mIndirectCommandBufferPool->RequestBuffer(completedFenceValue, totalMeshCount);
	mMeshBuffer = mMeshBufferPool->RequestBuffer(completedFenceValue, totalMeshCount);
	mSkinnedBuffer = mSkinnedBufferPool->RequestBuffer(completedFenceValue, GetModelsCount());

	int modelIdx = 0;
	for (auto& [name, model] : mModels)
	{
		if (model->IsSkinned())
		{
			mSkinnedBuffer->CopyElements(model->GetSkinnedConstants(), modelIdx);
			model->SetSkinnedCBAddress(mSkinnedBuffer->GetElementAddress(modelIdx));
			++modelIdx;
		}
	}

	int meshIdx = 0;
	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		for (auto& [name, mesh] : mMeshes[i])
		{
			mMeshBuffer->CopyElements(mesh->GetMeshConstants(), meshIdx);
			mesh->SetMeshCBAddress(mMeshBuffer->GetElementAddress(meshIdx));

			IndirectCommand indirectCmd;
			
			indirectCmd.MeshCBAddr = mesh->GetMeshCBAddress();
			indirectCmd.SkinnedCBAddr = mesh->GetSkinnedCBAddress();

			indirectCmd.DispatchMeshArgs.ThreadGroupCountX = ceilf(mesh->GetMeshletSize() * 1.f / WARP_SIZE);
			indirectCmd.DispatchMeshArgs.ThreadGroupCountY = 1;
			indirectCmd.DispatchMeshArgs.ThreadGroupCountZ = 1;

			mIndirectCommandBuffer->CopyElements(&indirectCmd, meshIdx);

			++meshIdx;
		}
	}
}

void Carol::SceneManager::ProcessNode(SceneNode* node, DirectX::XMMATRIX parentToRoot)
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