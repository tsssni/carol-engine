#pragma once
#include "Model.h"
#include "SkinnedData.h"
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

	class RenderData;

	class AssimpMaterial
	{
	public:
		XMFLOAT3 Emissive = {0.0f, 0.0f, 0.0f};
		XMFLOAT3 Ambient = {0.0f, 0.0f, 0.0f};
		XMFLOAT3 Diffuse = {0.0f, 0.0f, 0.0f};
		XMFLOAT3 Specular = {0.0f, 0.0f, 0.0f};
		XMFLOAT3 Reflection = {0.0f, 0.0f, 0.0f};
		float Shininess = 0.0f;
	};

	class AssimpVertex
	{
	public:
		XMFLOAT3 Pos = {0.0f ,0.0f ,0.0f};
		XMFLOAT3 Normal = {0.0f ,0.0f ,0.0f};
		XMFLOAT3 Tangent = {0.0f ,0.0f ,0.0f};
		XMFLOAT2 TexC = {0.0f ,0.0f};
		XMFLOAT3 Weights = { 0.0f,0.0f,0.0f };
		XMUINT4 BoneIndices = {0,0,0,0};
	};

	class AssimpModel : public Model
	{
	public:
		AssimpModel(RenderData* renderData);
	    AssimpModel(RenderData* renderData, wstring path, wstring textureDir, bool isSkinned, bool isTransparent);
		AssimpModel(const AssimpModel&) = delete;
		AssimpModel(AssimpModel&&) = delete;
		AssimpModel& operator=(const AssimpModel&) = delete;

		static void InitSkinnedCBHeap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
		static CircularHeap* GetSkinnedCBHeap();

		bool InitSucceeded();
		bool IsSkinned();
		void LoadFlatGround();
		void LoadSkyBox();

		void SetWorld(XMMATRIX world);

		void SetAnimation(wstring clipName);
		vector<wstring> GetAnimations();
		void UpdateAnimation();
	protected:
		void ProcessNode(aiNode* node, const aiScene* scene);
		void ProcessMesh(aiMesh* mesh, const aiScene* scene);
		void ReadMeshBones(uint32_t vertexOffset, aiMesh* mesh);
		void InsertBoneWeightToVertex(AssimpVertex& vertex, uint32_t boneIndex, float boneWeight);
		
		void LoadAssimpSkinnedData(const aiScene* scene);
		void ReadBones(aiNode* node, const aiScene* scene);
		void MarkSkinnedNodes(aiNode* node, aiNode* meshNode, wstring boneName);

		void InitBoneData(const aiScene* scene);
		void ReadBoneHierarchy(aiNode* node);
		void ReadBoneOffsets(aiNode* node, const aiScene* scene);
		void ReadSkinnedNodeTransforms(aiNode* node);
		void ReadAnimations(const aiScene* scene);

		void ReadMeshVerticesAndIndices(aiMesh* mesh);
		void ReadMeshMaterialAndTextures(MeshManager& submesh, aiMesh* mesh, const aiScene* scene);
		void ReadTexture(MeshManager& mesh, aiMaterial* matData);
		void LoadTexture(MeshManager& mesh, aiString aiPath, aiTextureType type);

		XMMATRIX aiMatrix4x4ToXM(aiMatrix4x4 aiM);
		aiMatrix4x4 XMToaiMatrix4x4(XMMATRIX xm);
	protected:
		RenderData* mRenderData;
		wstring mTextureDir;
		
		bool mInitSucceeded;
		bool mSkinned;
		bool mTransparent;

		vector<AssimpVertex> mVertices;
		vector<uint32_t> mIndices;

		static unique_ptr<CircularHeap> SkinnedCBHeap;
		unordered_map<wstring, bool> mSkinnedMark;
		vector<XMFLOAT4X4> mSkinnedNodeTransforms;
		uint32_t mSkinnedCount = 0;

		unordered_map<wstring, uint32_t> mBoneIndices;
		vector<int> mBoneMark;
		vector<int> mBoneHierarchy;
		vector<XMFLOAT4X4> mBoneOffsets;

		XMFLOAT4X4 mWorld;
		unordered_map<wstring, unique_ptr<AnimationClip>> mAnimations;
		wstring mClipName;
		
		float mTimePos = 0;
		vector<XMFLOAT4X4> mFinalTransforms;

		unique_ptr<SkinnedConstants> mSkinnedConstants;
		unique_ptr<HeapAllocInfo> mSkinnedCBAllocInfo;
	};

}
