#include <scene/scene.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/descriptor_allocator.h>
#include <scene/assimp.h>
#include <scene/timer.h>
#include <scene/texture.h>

namespace Carol
{
	using std::vector;
	using std::wstring;
	using std::make_unique;
	using namespace DirectX;
	
}

Carol::Octree::Octree(BoundingBox sceneBoundingBox, float looseFactor)
	:mLooseFactor(looseFactor)
{
	mRootNode = make_unique<OctreeNode>();
	mRootNode->BoundingBox = sceneBoundingBox;
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

	for (auto itr=node->Meshes.begin(); itr != node->Meshes.end(); ++itr)
	{
		if (*itr == mesh)
		{
			node->Meshes.erase(itr);
			break;
		}
	}
}

bool Carol::Octree::ProcessNode(OctreeNode* node, Mesh* mesh)
{
	auto b = mesh->GetBoundingBox();
	if (ExtendBoundingBox(node->BoundingBox).Contains(mesh->GetBoundingBox()) == ContainmentType::CONTAINS)
	{
		if (node->Children.size() == 0)
		{
			DevideBoundingBox(node);
		}

		for (int i = 0; i < 8; ++i)
		{
			if (ProcessNode(node->Children[i].get(), mesh))
			{
				return true;
			}
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
	}	
}

Carol::SceneNode::SceneNode()
{
	XMStoreFloat4x4(&Transformation, XMMatrixIdentity());
	XMStoreFloat4x4(&HistTransformation, XMMatrixIdentity());
	WorldAllocInfo = make_unique<HeapAllocInfo>();
}

Carol::Scene::Scene(std::wstring name, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, Heap* texHeap, Heap* uploadHeap, CbvSrvUavDescriptorAllocator* allocator)
	:mRootNode(make_unique<SceneNode>()),
	mOctree(make_unique<Octree>(XMVECTOR{-1000.0f,-1000.0f,-1000.0f},XMVECTOR{1000.0f,1000.0f,1000.0f})),
	mMeshes(MESH_TYPE_COUNT),
	mTexManager(make_unique<TextureManager>(device, cmdList, texHeap, uploadHeap, allocator)),
	mMeshCBHeap(make_unique<CircularHeap>(device, cmdList, true, 2048, sizeof(MeshConstants))), 
	mSkinnedCBHeap(make_unique<CircularHeap>(device, cmdList, true, 2048, sizeof(SkinnedConstants)))
{
	mRootNode->Name = name;
}

void Carol::Scene::DelayedDelete(uint32_t currFrame)
{
	mTexManager->DelayedDelete(currFrame);
	mMeshCBHeap->DelayedDelete(currFrame);
	mSkinnedCBHeap->DelayedDelete(currFrame);
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

void Carol::Scene::LoadModel(ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap, std::wstring name, std::wstring path, std::wstring textureDir, bool isSkinned)
{
	mRootNode->Children.push_back(make_unique<SceneNode>());
	auto& node = mRootNode->Children.back();
	node->Children.push_back(make_unique<SceneNode>());

	node->Name = name;
	mModels[name] = make_unique<AssimpModel>(cmdList, heap, uploadHeap, mTexManager.get(), node->Children[0].get(), path, textureDir, isSkinned);

	for (auto& meshMapPair : mModels[name]->GetMeshes())
	{
		auto& mesh = meshMapPair.second;
		mOctree->Insert(mesh.get());
	}
}

void Carol::Scene::LoadGround(ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap)
{
	mModels[L"Ground"] = make_unique<Model>();
	mModels[L"Ground"]->LoadGround(cmdList, heap, uploadHeap, mTexManager.get());
	
	mRootNode->Children.push_back(make_unique<SceneNode>());
	auto& node = mRootNode->Children.back();

	node->Name = L"Ground";
	node->Children.push_back(make_unique<SceneNode>());
	node->Children[0]->Meshes.push_back(mModels[L"Ground"]->GetMesh(L"Ground"));

	for (auto& meshMapPair : mModels[L"Ground"]->GetMeshes())
	{
		auto& mesh = meshMapPair.second;
		mOctree->Insert(mesh.get());
	}
}

void Carol::Scene::LoadSkyBox(ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap)
{
	mSkyBox = make_unique<Model>();
	mSkyBox->LoadSkyBox(cmdList, heap, uploadHeap, mTexManager.get());
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

Carol::vector<Carol::RenderNode>& Carol::Scene::GetMeshes(uint32_t type)
{
	if (type >= 0 && type < MESH_TYPE_COUNT)
	{
		return mMeshes[type];
	}
	else
	{
		return mMeshes[0];
	}
}

Carol::RenderNode Carol::Scene::GetSkyBox()
{
	return mSkyBoxNode;
}

void Carol::Scene::SetWorld(std::wstring modelName, DirectX::XMMATRIX world)
{
	for (auto& node : mRootNode->Children)
	{
		if (node->Name == modelName)
		{
			XMStoreFloat4x4(&node->Transformation, world);
			node->Modified = true;
		}
	}
}

void Carol::Scene::SetAnimationClip(std::wstring modelName, std::wstring clipName)
{
	mModels[modelName]->SetAnimationClip(clipName);
}

void Carol::Scene::Update(Timer& timer)
{
	for (auto& meshes : mMeshes)
	{
		meshes.clear();
	}

	for (auto& modelMapPair : mModels)
	{
		auto& model = modelMapPair.second;
		if (model->IsSkinned())
		{
			model->UpdateAnimationClip(timer, mSkinnedCBHeap.get());
		}
	}

	ProcessNode(mRootNode.get(), XMMatrixIdentity(), XMMatrixIdentity(), false);
	UpdateSkyBox();
}

void Carol::Scene::ProcessNode(SceneNode* node, DirectX::XMMATRIX parentToRoot, DirectX::XMMATRIX histParentToRoot, bool modified)
{
	XMMATRIX toParent = XMLoadFloat4x4(&node->Transformation);
	XMMATRIX histToParent = XMLoadFloat4x4(&node->HistTransformation);
	XMMATRIX world = toParent * parentToRoot;
	XMMATRIX histWorld = histToParent * histParentToRoot;

	if (node->Modified == true)
	{
		modified = true;
	}

	for(auto& mesh : node->Meshes)
	{
		MeshConstants meshConstants;
		Material mat = mesh->GetMaterial();

		XMStoreFloat4x4(&meshConstants.World, XMMatrixTranspose(world));
		XMStoreFloat4x4(&meshConstants.HistWorld, XMMatrixTranspose(histWorld));
		meshConstants.FresnelR0 = mat.FresnelR0;
		meshConstants.Roughness = mat.Roughness;

		mMeshCBHeap->DeleteResource(node->WorldAllocInfo.get());
		mMeshCBHeap->CreateResource(node->WorldAllocInfo.get());
		mMeshCBHeap->CopyData(node->WorldAllocInfo.get(), &meshConstants);

		RenderNode renderNode;
		renderNode.Mesh = mesh;
		renderNode.WorldGPUVirtualAddress = mMeshCBHeap->GetGPUVirtualAddress(node->WorldAllocInfo.get());

		uint32_t type = mesh->IsSkinned() | (mesh->IsTransparent() << 1);
		mMeshes[type].push_back(renderNode);

		if (modified)
		{
			if (!mesh->TransformBoundingBox(world))
			{
				mOctree->Delete(mesh);
				mOctree->Insert(mesh);
			}
		}
	}

	node->HistTransformation = node->Transformation;

	for (auto& child : node->Children)
	{
		ProcessNode(child.get(), world, histWorld, modified);
	}

	node->Modified = false;
}

void Carol::Scene::UpdateSkyBox()
{
	static HeapAllocInfo skyBoxInfo;
	static SceneNode skyBoxNode;

	mMeshCBHeap->DeleteResource(&skyBoxInfo);
	mMeshCBHeap->CreateResource(&skyBoxInfo);
	mMeshCBHeap->CopyData(&skyBoxInfo, &skyBoxNode.Transformation);

	mSkyBoxNode.Mesh = mSkyBox->GetMesh(L"SkyBox");
	mSkyBoxNode.WorldGPUVirtualAddress = mMeshCBHeap->GetGPUVirtualAddress(&skyBoxInfo);
}
