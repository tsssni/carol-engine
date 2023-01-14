#include <scene/octree.h>
#include <scene/model.h>
#include <scene/camera.h>
#include <utils/common.h>

namespace Carol
{
	using std::vector;
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