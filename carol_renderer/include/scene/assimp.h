#pragma once
#include <scene/model.h>
#include <assimp/scene.h>
#include <DirectXMath.h>
#include <memory>
#include <unordered_map>
#include <string>
#include <string_view>

namespace Carol
{
	class SceneNode;
	class HeapManager;
	class DescriptorManager;

	class AssimpModel : public Model
	{
	public:
		AssimpModel(
			SceneNode* rootNode,
		    std::wstring_view path,
			std::wstring_view textureDir,
			bool isSkinned,
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmdList,
			Heap* defaultBuffersHeap,
			Heap* uploadBuffersHeap,
			DescriptorManager* descriptorManager,
			TextureManager* textureManager);
		AssimpModel(const AssimpModel&) = delete;
		AssimpModel(AssimpModel&&) = delete;
		AssimpModel& operator=(const AssimpModel&) = delete;

	protected:
		void ProcessNode(
			aiNode* node,
			SceneNode* sceneNode,
			const aiScene* scene,
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmdList,
			Heap* defaultBuffersHeap,
			Heap* uploadBuffersHeap,
			DescriptorManager* descriptorManager);
		Mesh* ProcessMesh(
			aiMesh* mesh,
			const aiScene* scene,
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmdList,
			Heap* defaultBuffersHeap,
			Heap* uploadBuffersHeap,
			DescriptorManager* descriptorManager);
		
		void ReadBoneHierachy(aiNode* node);
		void ReadBoneOffsets(const aiScene* scene);
		void ReadAnimations(const aiScene* scene);

		void ReadMeshVerticesAndIndices(
			std::vector<Vertex>& vertices,
			std::vector<uint32_t>& indices,
			aiMesh* mesh);
		void ReadMeshMaterialAndTextures(
			Mesh* mesh,
			aiMesh* aimesh,
			const aiScene* scene,
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmdList,
			Heap* defaultBuffersHeap,
			Heap* uploadBuffersHeap,
			DescriptorManager* descriptorManager);
		void ReadMeshBones(std::vector<Vertex>& vertices, aiMesh* mesh);
		void InsertBoneWeightToVertex(Vertex& vertex, uint32_t boneIndex, float boneWeight);

		void LoadTexture(
			Mesh* mesh,
			aiString aiPath,
			aiTextureType type,
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmdList,
			Heap* defaultBuffersHeap,
			Heap* uploadBuffersHeap,
			DescriptorManager* descriptorManager);

	protected:
		std::unordered_map<std::wstring, uint32_t> mBoneIndices;
	};
}
