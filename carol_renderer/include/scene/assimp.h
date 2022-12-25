#pragma once
#include <scene/model.h>
#include <assimp/scene.h>
#include <DirectXMath.h>
#include <memory>
#include <unordered_map>
#include <string>

namespace Carol
{
	class Heap;

	class AssimpModel : public Model
	{
	public:
		AssimpModel();
	    AssimpModel(ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap, std::wstring path, std::wstring textureDir, bool isSkinned);
		AssimpModel(const AssimpModel&) = delete;
		AssimpModel(AssimpModel&&) = delete;
		AssimpModel& operator=(const AssimpModel&) = delete;

	protected:
		void ProcessNode(aiNode* node, const aiScene* scene);
		void ProcessMesh(aiMesh* mesh, const aiScene* scene);
		void ReadMeshBones(uint32_t vertexOffset, aiMesh* mesh);
		void InsertBoneWeightToVertex(Vertex& vertex, uint32_t boneIndex, float boneWeight);
		
		void LoadAssimpSkinnedData(const aiScene* scene);
		void ReadBones(aiNode* node, const aiScene* scene);
		void MarkSkinnedNodes(aiNode* node, aiNode* meshNode, std::wstring boneName);

		void InitBoneData(const aiScene* scene);
		void ReadBoneHierarchy(aiNode* node);
		void ReadBoneOffsets(aiNode* node, const aiScene* scene);
		void ReadSkinnedNodeTransforms(aiNode* node);
		void ReadAnimations(const aiScene* scene);

		void ReadMeshVerticesAndIndices(aiMesh* mesh);
		void ReadMeshMaterialAndTextures(Mesh* submesh, aiMesh* mesh, const aiScene* scene);
		void ReadTexture(Mesh* mesh, aiMaterial* matData);
		void LoadTexture(Mesh* mesh, aiString aiPath, aiTextureType type);

		DirectX::XMMATRIX aiMatrix4x4ToXM(aiMatrix4x4 aiM);
		aiMatrix4x4 XMToaiMatrix4x4(DirectX::XMMATRIX xm);
	protected:
		ID3D12GraphicsCommandList* mCommandList;

		std::unordered_map<std::wstring, bool> mSkinnedMark;
		std::vector<DirectX::XMFLOAT4X4> mSkinnedNodeTransforms;
		uint32_t mSkinnedCount = 0;

		std::unordered_map<std::wstring, uint32_t> mBoneIndices;
		std::vector<int> mBoneMark;
	};

}
