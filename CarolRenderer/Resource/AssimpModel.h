#pragma once
#include "Model.h"
#include "SkinnedData.h"
#include "Texture.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <DirectXMath.h>
#include <memory>
#include <unordered_map>
#include <string>

using namespace DirectX;
using std::unique_ptr;
using std::unordered_map;
using std::wstring;

namespace Carol
{
	class AssimpMaterial
	{
	public:
		int DiffuseMapIndex = -1;
		int NormalMapIndex = -1;
		int MatTBIndex = -1;

		XMFLOAT3 Emissive = {0.0f, 0.0f, 0.0f};
		XMFLOAT3 Ambient = {0.0f, 0.0f, 0.0f};
		XMFLOAT3 Diffuse = {0.0f, 0.0f, 0.0f};
		XMFLOAT3 Specular = {0.0f, 0.0f, 0.0f};
		XMFLOAT3 Reflection = {0.0f, 0.0f, 0.0f};
		float Shininess = 0.0f;
	};

	class AssimpModel : public Model
	{
	public:
		class AssimpVertex
		{
		public:
			XMFLOAT3 Pos = {0.0f ,0.0f ,0.0f};
			XMFLOAT3 Normal = {0.0f ,0.0f ,0.0f};
			XMFLOAT3 Tangent = {0.0f ,0.0f ,0.0f};
			XMFLOAT2 TexC = {0.0f ,0.0f};
		};

	    void LoadAssimpModel(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, wstring path, uint32_t matOffset, uint32_t& texOffset);
		vector<AssimpMaterial>& GetMaterials();
		unordered_map<wstring, uint32_t>& GetTextures();

		static unique_ptr<AssimpModel> GetFlatGround(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, uint32_t matOffset, uint32_t& texOffset);

	protected:
		void ProcessNode(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, vector<AssimpVertex>& vertices, vector<uint32_t>& indices, uint32_t matOffset, uint32_t& texOffset, aiNode* node, const aiScene* scene);
		void ProcessMesh(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, vector<AssimpVertex>& vertices, vector<uint32_t>& indices, uint32_t matOffset, uint32_t& texOffset, aiMesh* mesh, const aiScene* scene);
		
		void ReadMeshVerticesAndIndices(vector<AssimpVertex>& vertices, vector<uint32_t>& indices, aiMesh* mesh);
		void ReadMeshMaterialAndTextures(uint32_t matOffset, uint32_t& texOffset, aiMesh* mesh, const aiScene* scene);
		void ReadTexture(uint32_t& texOffset, aiMaterial* matData, AssimpMaterial& mat, aiTextureType type);

	protected:
		vector<AssimpMaterial> mMaterials;
		unordered_map<wstring, uint32_t> mTextureIndexMap;
	};

	class SkinnedAssimpModel : public AssimpModel
	{
	public:
		class SkinnedAssimpVertex : public AssimpVertex
		{
		public:
			XMFLOAT3 Weights = { 0.0f,0.0f,0.0f };
			XMUINT4 BoneIndices = {0,0,0,0};
		};

		void LoadSkinnedAssimpModel(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, wstring path, uint32_t matOffset, uint32_t& texOffset);
		SkinnedData& GetSkinnedData();
	protected:
		void ReadBones(aiNode* node, const aiScene* scene);
		void ReadBoneHierachy(aiNode* node, uint32_t& boneCount);

		void ProcessNode(vector<SkinnedAssimpVertex>& vertices, vector<uint32_t>& indices, uint32_t matOffset, uint32_t& texOffset, aiNode* node, const aiScene* scene);
		void ProcessMesh(vector<SkinnedAssimpVertex>& vertices, vector<uint32_t>& indices, uint32_t matOffset, uint32_t& texOffset, aiMesh* mesh, const aiScene* scene);
		
		void ReadMeshVerticesAndIndices(vector<SkinnedAssimpVertex>& vertices, vector<uint32_t>& indices, aiMesh* mesh);
		void ReadMeshBones(vector<SkinnedAssimpVertex>& vertices, uint32_t vertexOffset, aiMesh* mesh);
		void InsertBoneWeightToVertex(SkinnedAssimpVertex& vertex, uint32_t boneIndex, float boneWeight);

		void ReadAnimation(const aiScene* scene);
	protected:
		SkinnedData mSkinnedData;

		vector<int> mBoneHierachy;
		vector<XMFLOAT4X4> mBoneOffsets;
		unordered_map<wstring, int> mBoneIndices;
		vector<bool> mBoneInited;

		unordered_map<wstring, AnimationClip> mAnimationClips;
	};
}
