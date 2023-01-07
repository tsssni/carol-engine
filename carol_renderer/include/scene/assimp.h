#pragma once
#include <scene/model.h>
#include <scene/light.h>
#include <assimp/scene.h>
#include <DirectXMath.h>
#include <memory>
#include <unordered_map>
#include <string>

namespace Carol
{
	class Heap;
	class DescriptorAllocator;
	class SceneNode;
	class TextureManager;

	class AssimpModel : public Model
	{
	public:
		AssimpModel();
	    AssimpModel(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap, DescriptorAllocator* allocator, TextureManager* texManager, SceneNode* rootNode, std::wstring path, std::wstring textureDir, bool isSkinned);
		AssimpModel(const AssimpModel&) = delete;
		AssimpModel(AssimpModel&&) = delete;
		AssimpModel& operator=(const AssimpModel&) = delete;

	protected:
		void ProcessNode(aiNode* node, SceneNode* sceneNode, const aiScene* scene);
		Mesh* ProcessMesh(aiMesh* mesh, const aiScene* scene);
		
		void LoadAssimpSkinnedData(const aiScene* scene);
		void ReadBones(aiNode* node, const aiScene* scene);
		void MarkSkinnedNodes(aiNode* node, aiNode* meshNode, std::wstring boneName);

		void InitBoneData(const aiScene* scene);
		void ReadBoneHierarchy(aiNode* node);
		void ReadBoneOffsets(aiNode* node, const aiScene* scene);
		void ReadAnimations(const aiScene* scene);

		void ReadMeshVerticesAndIndices(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, aiMesh* mesh);
		void ReadMeshMaterialAndTextures(Mesh* submesh, aiMesh* mesh, const aiScene* scene);
		void ReadMeshBones(std::vector<Vertex>& vertices, aiMesh* mesh);
		void InsertBoneWeightToVertex(Vertex& vertex, uint32_t boneIndex, float boneWeight);

		void ReadTexture(Mesh* mesh, aiMaterial* matData);
		void LoadTexture(Mesh* mesh, aiString aiPath, aiTextureType type);


		DirectX::XMMATRIX aiMatrix4x4ToXM(aiMatrix4x4 aiM);
		aiMatrix4x4 XMToaiMatrix4x4(DirectX::XMMATRIX xm);
	protected:
		
		TextureManager* mTexManager;

		std::unordered_map<std::wstring, bool> mSkinnedMark;
		uint32_t mSkinnedCount = 0;

		std::unordered_map<std::wstring, uint32_t> mBoneIndices;
		std::vector<int> mBoneMark;
	};
}
