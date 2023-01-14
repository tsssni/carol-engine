#pragma once
#include <DirectXCollision.h>
#include <vector>;
#include <memory>

namespace Carol {
	class Mesh;
	class Camera;

	class OctreeNode
	{
	public:
		std::vector<Mesh*> Meshes;
		std::vector<std::unique_ptr<OctreeNode>> Children;
		DirectX::BoundingBox BoundingBox;
		DirectX::BoundingBox LooseBoundingBox;
	};

	class Octree
	{
	public:
		Octree(DirectX::BoundingBox sceneBoundingBox, float looseFactor = 1.5f);
		Octree(DirectX::XMVECTOR boxMin, DirectX::XMVECTOR boxMax, float looseFactor = 1.5f);

		void Insert(Mesh* mesh);
		void Delete(Mesh* mesh);
		void Contain(Camera* camera, std::vector<std::vector<Mesh*>>& meshes);
	protected:
		bool ProcessNode(OctreeNode* node, Mesh* mesh);
		void ProcessContainment(OctreeNode* node, Camera* camera, std::vector<std::vector<Mesh*>>& meshes);

		DirectX::BoundingBox ExtendBoundingBox(const DirectX::BoundingBox& box);
		void DevideBoundingBox(OctreeNode* node);

		std::unique_ptr<OctreeNode> mRootNode;
		float mLooseFactor;
	};
}

