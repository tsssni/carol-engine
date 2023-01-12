#include <scene/scene.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/descriptor.h>
#include <scene/assimp.h>
#include <scene/timer.h>
#include <scene/camera.h>
#include <scene/texture.h>

namespace Carol
{
	using std::vector;
	using std::wstring;
	using std::unordered_map;
	using std::unique_ptr;
	using std::make_unique;
	using namespace DirectX;
	
}

Carol::Octree::Octree(BoundingBox sceneBoundingBox, float looseFactor)
	:mLooseFactor(looseFactor)
{
	mRootNode = make_unique<OctreeNode>();
	mRootNode->BoundingBox = sceneBoundingBox;
	mRootNode->LooseBoundingBox = ExtendBoundingBox(mRootNode->BoundingBox);
}
Carol::Octree::Octree(DirectX::XMVECTOR boxMin, DirectX::XMVECTOR boxMax, float looseFactor)
{
	BoundingBox box;
	BoundingBox::CreateFromPoints(box, boxMin, boxMax);

	this->Octree::Octree(box, looseFactor);
}
void Carol::Octree::Insert(Mesh* mesh)
{
	ProcessNode(mRootNode.get(), mesh);
}

void Carol::Octree::Delete(Mesh* mesh)
{
	auto node = mesh->GetOctreeNode();

	for (auto itr = node->Meshes.begin(); itr != node->Meshes.end(); ++itr)
	{
		if (*itr == mesh)
		{
			node->Meshes.erase(itr);
			break;
		}
	}
}

void Carol::Octree::Contain(Camera* camera, std::vector<std::vector<Mesh*>>& meshes)
{
	ProcessContainment(mRootNode.get(), camera, meshes);
}

bool Carol::Octree::ProcessNode(OctreeNode* node, Mesh* mesh)
{
	auto b = mesh->GetBoundingBox();
	if (node->LooseBoundingBox.Contains(mesh->GetBoundingBox()) == ContainmentType::CONTAINS)
	{
		if (node->Children.size() == 0)
		{
			DevideBoundingBox(node);
		}

		node->Meshes.push_back(mesh);
		mesh->SetOctreeNode(node);
		return true;
	}
	else
	{
	return false;
	}
}

void Carol::Octree::ProcessContainment(OctreeNode* node, Camera* camera, vector<vector<Mesh*>>& meshes)
{
	if (camera->Contains(node->LooseBoundingBox))
	{
		for (auto& mesh : node->Meshes)
		{
			if (camera->Contains(mesh->GetBoundingBox()))
			{
				uint32_t type = mesh->IsSkinned() | (mesh->IsTransparent() << 1);
				meshes[type].push_back(mesh);
			}
		}

		for (int i = 0; i < node->Children.size(); ++i)
		{
			ProcessContainment(node->Children[i].get(), camera, meshes);
		}
	}
}

Carol::BoundingBox Carol::Octree::ExtendBoundingBox(const DirectX::BoundingBox& box)
{
	BoundingBox extendedBox = box;
	auto extent = XMLoadFloat3(&extendedBox.Extents);
	extent *= mLooseFactor;
	XMStoreFloat3(&extendedBox.Extents, extent);
	return extendedBox;
}

void Carol::Octree::DevideBoundingBox(OctreeNode* node)
{
	node->Children.resize(8);
	auto center = XMLoadFloat3(&node->BoundingBox.Center);
	XMVECTOR extents;
	static float dx[8] = { -1,1,-1,1,-1,1,-1,1 };
	static float dy[8] = { -1,-1,1,1,-1,-1,1,1 };
	static float dz[8] = { -1,-1,-1,-1,1,1,1,1 };
	for (int i = 0; i < 8; ++i)
	{
		extents = XMLoadFloat3(GetRvaluePtr(XMFLOAT3{
			node->BoundingBox.Extents.x * dx[i],
			node->BoundingBox.Extents.y * dy[i],
			node->BoundingBox.Extents.z * dz[i]
			}));
		node->Children[i] = make_unique<OctreeNode>();
		BoundingBox::CreateFromPoints(node->Children[i]->BoundingBox, center, center + extents);
		node->Children[i]->LooseBoundingBox = ExtendBoundingBox(node->Children[i]->BoundingBox);
	}
}

Carol::SceneNode::SceneNode()
{
	XMStoreFloat4x4(&Transformation, XMMatrixIdentity());
}

Carol::Scene::Scene(std::wstring name, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, HeapManager* heapManager, DescriptorManager* descriptorManager)
	:mRootNode(make_unique<SceneNode>()),
	mMeshes(MESH_TYPE_COUNT),
	mDevice(device),
	mCommandList(cmdList),
	mHeapManager(heapManager),
	mDescriptorManager(descriptorManager),
	mTexManager(make_unique<TextureManager>(device, cmdList, heapManager, descriptorManager))
{
	mRootNode->Name = name;
}

void Carol::Scene::DelayedDelete(uint32_t currFrame)
{
	mTexManager->DelayedDelete(currFrame);
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
		isSkinned,
		mDevice,
		mCommandList,
		mHeapManager,
		mDescriptorManager);

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
	mModels[L"Ground"] = make_unique<Model>(mDevice, mCommandList, mHeapManager, mDescriptorManager);
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
	mSkyBox = make_unique<Model>(mDevice, mCommandList, mHeapManager, mDescriptorManager);
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
