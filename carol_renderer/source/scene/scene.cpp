#include <scene/scene.h>
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
	using std::wstring_view;
	using std::unordered_map;
	using std::unique_ptr;
	using std::make_unique;
	using namespace DirectX;
	using Microsoft::WRL::ComPtr;
}

Carol::Scene::Scene(
	wstring_view name,
	ID3D12Device* device,
	Heap* defaultBuffersHeap,
	Heap* uploadBuffersHeap,
	DescriptorManager* descriptorManager)
	:mRootNode(make_unique<SceneNode>()),
	mMeshes(MESH_TYPE_COUNT)
{
	mRootNode->Name = name;
	InitBuffers(device, defaultBuffersHeap, uploadBuffersHeap, descriptorManager);
}

void Carol::Scene::InitBuffers(
	ID3D12Device* device,
	Heap* defaultBuffersHeap,
	Heap* uploadBuffersHeap,
	DescriptorManager* descriptorManager)
{
	mInstanceFrustumCulledMarkBuffer = make_unique<RawBuffer>(
		2 << 16,
		device,
		defaultBuffersHeap,
		descriptorManager,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mInstanceOcclusionCulledMarkBuffer = make_unique<RawBuffer>(
		2 << 16,
		device,
		defaultBuffersHeap,
		descriptorManager,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mInstanceCulledMarkBuffer = make_unique<RawBuffer>(
		2 << 16,
		device,
		defaultBuffersHeap,
		descriptorManager,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mIndirectCommandBufferPool = make_unique<StructuredBufferPool>(
		1024,
		sizeof(IndirectCommand),
		device,
		uploadBuffersHeap,
		descriptorManager,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_NONE,
		false);

	mMeshBufferPool = make_unique<StructuredBufferPool>(
		1024,
		sizeof(MeshConstants),
		device,
		uploadBuffersHeap,
		descriptorManager,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_NONE,
		true);

	mSkinnedBufferPool = make_unique<StructuredBufferPool>(
		1024,
		sizeof(SkinnedConstants),
		device,
		uploadBuffersHeap,
		descriptorManager,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_NONE,
		true);
}

Carol::vector<Carol::wstring_view> Carol::Scene::GetAnimationClips(wstring_view modelName)const
{
	return mModels.at(modelName.data())->GetAnimationClips();
}

Carol::vector<Carol::wstring_view> Carol::Scene::GetModelNames()const
{
	vector<wstring_view> models;
	for (auto& [name, model] : mModels)
	{
		models.push_back(wstring_view(name.c_str(), name.size()));
	}

	return models;
}

bool Carol::Scene::IsAnyTransparentMeshes()const
{
	return mMeshes[TRANSPARENT_STATIC].size() + mMeshes[TRANSPARENT_SKINNED].size();
}

void Carol::Scene::LoadModel(
	wstring_view name,
	wstring_view path,
	wstring_view textureDir,
	bool isSkinned,
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	Heap* defaultBuffersHeap,
	Heap* uploadBuffersHeap,
	DescriptorManager* descriptorManager,
	TextureManager* textureManager)
{
	mRootNode->Children.push_back(make_unique<SceneNode>());
	auto& node = mRootNode->Children.back();
	node->Children.push_back(make_unique<SceneNode>());

	node->Name = name;
	mModels[node->Name] = make_unique<AssimpModel>(
		node.get(),
		path,
		textureDir,
		isSkinned,
		device,
		cmdList,
		defaultBuffersHeap,
		uploadBuffersHeap,
		descriptorManager,
		textureManager);

	for (auto& [name, mesh] : mModels[node->Name]->GetMeshes())
	{
		wstring meshName = node->Name + L'_' + name;
		uint32_t type = uint32_t(mesh->IsSkinned()) | (uint32_t(mesh->IsTransparent()) << 1);
		mMeshes[type][meshName] = mesh.get();
	}
}

void Carol::Scene::LoadSkyBox(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	Heap* defaultBuffersHeap,
	Heap* uploadBuffersHeap,
	DescriptorManager* descriptorManager,
	TextureManager* textureManager)
{
	mSkyBox = make_unique<Model>(textureManager);
	mSkyBox->LoadSkyBox(
		device,
		cmdList,
		defaultBuffersHeap,
		uploadBuffersHeap,
		descriptorManager);
}

void Carol::Scene::UnloadModel(wstring_view modelName)
{
	for (auto itr = mRootNode->Children.begin(); itr != mRootNode->Children.end(); ++itr)
	{
		if ((*itr)->Name == modelName)
		{
			mRootNode->Children.erase(itr);
			break;
		}
	}

	wstring name(modelName);

	for (auto& [meshName, mesh] : mModels[name]->GetMeshes())
	{
		wstring modelMeshName = name + L"_" + meshName;
		uint32_t type = uint32_t(mesh->IsSkinned()) | (uint32_t(mesh->IsTransparent()) << 1);
		mMeshes[type].erase(modelMeshName);
	}

	mModels.erase(name);
}

void Carol::Scene::ReleaseIntermediateBuffers()
{
	for (auto& [name ,model] : mModels)
	{
		model->ReleaseIntermediateBuffers();
	}
}

void Carol::Scene::ReleaseIntermediateBuffers(wstring_view modelName)
{
	mModels[modelName.data()]->ReleaseIntermediateBuffers();
}

uint32_t Carol::Scene::GetMeshesCount(MeshType type)const
{
	return mMeshes[type].size();
}

uint32_t Carol::Scene::GetModelsCount()const
{
	return mModels.size();
}

const Carol::Mesh* Carol::Scene::GetSkyBox()const
{
	return mSkyBox->GetMesh(L"SkyBox");
}

void Carol::Scene::SetWorld(wstring_view modelName, DirectX::XMMATRIX world)
{
	for (auto& node : mRootNode->Children)
	{
		if (node->Name == modelName)
		{
			XMStoreFloat4x4(&node->Transformation, world);
		}
	}
}

void Carol::Scene::SetAnimationClip(wstring_view modelName, wstring_view clipName)
{
	mModels[modelName.data()]->SetAnimationClip(clipName);
}

void Carol::Scene::Contain(Camera* camera, std::vector<std::vector<Mesh*>>& meshes)
{
}

void Carol::Scene::ClearCullMark(ID3D12GraphicsCommandList* cmdList)
{
	static const uint32_t clear0 = 0;
	static const uint32_t clear1 = 0xffffffff;
	cmdList->ClearUnorderedAccessViewUint(mInstanceFrustumCulledMarkBuffer->GetGpuUav(), mInstanceFrustumCulledMarkBuffer->GetCpuUav(), mInstanceFrustumCulledMarkBuffer->Get(), &clear0, 0, nullptr);
	cmdList->ClearUnorderedAccessViewUint(mInstanceOcclusionCulledMarkBuffer->GetGpuUav(), mInstanceOcclusionCulledMarkBuffer->GetCpuUav(), mInstanceOcclusionCulledMarkBuffer->Get(), &clear1, 0, nullptr);
	cmdList->ClearUnorderedAccessViewUint(mInstanceCulledMarkBuffer->GetGpuUav(), mInstanceCulledMarkBuffer->GetCpuUav(), mInstanceCulledMarkBuffer->Get(), &clear1, 0, nullptr);

	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		for (auto& [name, mesh] : mMeshes[i])
		{
			mesh->ClearCullMark(cmdList);
		}
	}
}

uint32_t Carol::Scene::GetMeshCBStartOffet(MeshType type)const
{
	return mMeshStartOffset[type];
}

uint32_t Carol::Scene::GetMeshBufferIdx()const
{
	return mMeshBuffer->GetGpuSrvIdx();
}

uint32_t Carol::Scene::GetCommandBufferIdx()const
{
	return mIndirectCommandBuffer->GetGpuSrvIdx();
}

uint32_t Carol::Scene::GetInstanceFrustumCulledMarkBufferIdx()const
{
	return mInstanceFrustumCulledMarkBuffer->GetGpuUavIdx();
}

uint32_t Carol::Scene::GetInstanceOcclusionCulledMarkBufferIdx()const
{
	return mInstanceOcclusionCulledMarkBuffer->GetGpuUavIdx();
}

uint32_t Carol::Scene::GetInstanceCulledMarkBufferIdx() const
{
	return mInstanceCulledMarkBuffer->GetGpuUavIdx();
}

void Carol::Scene::Update(Timer* timer, uint64_t cpuFenceValue, uint64_t completedFenceValue)
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

	mMeshBuffer->CopyElements(GetSkyBox()->GetMeshConstants(), meshIdx);
    mSkyBox->SetMeshCBAddress(L"SkyBox", mMeshBuffer->GetElementAddress(meshIdx));
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