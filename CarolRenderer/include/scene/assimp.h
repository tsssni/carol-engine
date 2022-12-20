#pragma once
#include <scene/model.h>
#include <assimp/scene.h>
#include <DirectXMath.h>
#include <memory>
#include <unordered_map>
#include <string>

namespace Carol
{
	class GlobalResources;
	class SkinnedConstants;
	class AnimationClip;

	class AssimpMaterial
	{
	public:
		DirectX::XMFLOAT3 Emissive = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 Ambient = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 Diffuse = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 Specular = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 Reflection = {0.0f, 0.0f, 0.0f};
		float Shininess = 0.0f;
	};

	class AssimpVertex
	{
	public:
		DirectX::XMFLOAT3 Pos = {0.0f ,0.0f ,0.0f};
		DirectX::XMFLOAT3 Normal = {0.0f ,0.0f ,0.0f};
		DirectX::XMFLOAT3 Tangent = {0.0f ,0.0f ,0.0f};
		DirectX::XMFLOAT2 TexC = {0.0f ,0.0f};
		DirectX::XMFLOAT3 Weights = { 0.0f,0.0f,0.0f };
		DirectX::XMUINT4 BoneIndices = {0,0,0,0};
	};

	class AssimpModel : public Model
	{
	public:
		AssimpModel(GlobalResources* globalResources);
	    AssimpModel(GlobalResources* globalResources, std::wstring path, std::wstring textureDir, bool isSkinned, bool isTransparent);
		AssimpModel(const AssimpModel&) = delete;
		AssimpModel(AssimpModel&&) = delete;
		AssimpModel& operator=(const AssimpModel&) = delete;

		static void InitSkinnedCBHeap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
		static CircularHeap* GetSkinnedCBHeap();

		bool InitSucceeded();
		bool IsSkinned();
		void LoadFlatGround();
		void LoadSkyBox();

		void SetWorld(DirectX::XMMATRIX world);

		void SetAnimation(std::wstring clipName);
		std::vector<std::wstring> GetAnimations();
		void UpdateAnimation();
	protected:
		void ProcessNode(aiNode* node, const aiScene* scene);
		void ProcessMesh(aiMesh* mesh, const aiScene* scene);
		void ReadMeshBones(uint32_t vertexOffset, aiMesh* mesh);
		void InsertBoneWeightToVertex(AssimpVertex& vertex, uint32_t boneIndex, float boneWeight);
		
		void LoadAssimpSkinnedData(const aiScene* scene);
		void ReadBones(aiNode* node, const aiScene* scene);
		void MarkSkinnedNodes(aiNode* node, aiNode* meshNode, std::wstring boneName);

		void InitBoneData(const aiScene* scene);
		void ReadBoneHierarchy(aiNode* node);
		void ReadBoneOffsets(aiNode* node, const aiScene* scene);
		void ReadSkinnedNodeTransforms(aiNode* node);
		void ReadAnimations(const aiScene* scene);

		void ReadMeshVerticesAndIndices(aiMesh* mesh);
		void ReadMeshMaterialAndTextures(MeshManager& submesh, aiMesh* mesh, const aiScene* scene);
		void ReadTexture(MeshManager& mesh, aiMaterial* matData);
		void LoadTexture(MeshManager& mesh, aiString aiPath, aiTextureType type);

		DirectX::XMMATRIX aiMatrix4x4ToXM(aiMatrix4x4 aiM);
		aiMatrix4x4 XMToaiMatrix4x4(DirectX::XMMATRIX xm);
	protected:
		GlobalResources* mGlobalResources;
		std::wstring mTextureDir;
		
		bool mInitSucceeded;
		bool mSkinned;
		bool mTransparent;

		std::vector<AssimpVertex> mVertices;
		std::vector<uint32_t> mIndices;

		static std::unique_ptr<CircularHeap> SkinnedCBHeap;
		std::unordered_map<std::wstring, bool> mSkinnedMark;
		std::vector<DirectX::XMFLOAT4X4> mSkinnedNodeTransforms;
		uint32_t mSkinnedCount = 0;

		std::unordered_map<std::wstring, uint32_t> mBoneIndices;
		std::vector<int> mBoneMark;
		std::vector<int> mBoneHierarchy;
		std::vector<DirectX::XMFLOAT4X4> mBoneOffsets;

		DirectX::XMFLOAT4X4 mWorld;
		std::unordered_map<std::wstring, std::unique_ptr<AnimationClip>> mAnimations;
		std::wstring mClipName;
		
		float mTimePos = 0;
		std::vector<DirectX::XMFLOAT4X4> mFinalTransforms;

		std::unique_ptr<SkinnedConstants> mSkinnedConstants;
		std::unique_ptr<HeapAllocInfo> mSkinnedCBAllocInfo;
	};

}
