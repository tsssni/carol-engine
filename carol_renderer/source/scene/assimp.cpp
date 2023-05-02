#include <carol.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <algorithm>
#include <fstream>
#include <span>

#define aiProcess_Static aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices | aiProcess_FixInfacingNormals | aiProcess_PreTransformVertices | aiProcess_ConvertToLeftHanded
#define aiProcess_Skinned aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices | aiProcess_FixInfacingNormals | aiProcess_LimitBoneWeights | aiProcess_ConvertToLeftHanded

namespace Carol {
	using std::vector;
	using std::wstring;
	using std::wstring_view;
	using std::unique_ptr;
	using std::make_unique;
	using std::unordered_map;
	using std::pair;
	using std::span;
	using namespace DirectX;
}

Carol::XMMATRIX ai2XM(const aiMatrix4x4& aiM)
{
	Carol::XMFLOAT4X4 M =
	{
		aiM.a1,aiM.b1,aiM.c1,aiM.d1,
		aiM.a2,aiM.b2,aiM.c2,aiM.d2,
		aiM.a3,aiM.b3,aiM.c3,aiM.d3,
		aiM.a4,aiM.b4,aiM.c4,aiM.d4
	};

	return Carol::XMLoadFloat4x4(&M);
}

Carol::XMVECTOR ai2XM(const aiColor3D& aiC)
{
	return { aiC.r,aiC.g,aiC.b };
}

Carol::XMVECTOR ai2XM(const aiVector2D& aiV)
{
	return { aiV.x,aiV.y };
}

Carol::XMVECTOR ai2XM(const aiVector3D& aiV)
{
	return { aiV.x,aiV.y,aiV.z };
}

Carol::AssimpModel::AssimpModel(
	SceneNode* rootNode,
	wstring_view path,
	wstring_view textureDir,
	bool isSkinned)
	:Model()
{
	Assimp::Importer mImporter;
	const aiScene* scene = mImporter.ReadFile(WStringToString(path), isSkinned ? aiProcess_Skinned : aiProcess_Static);
	assert(scene);

	mTexDir = textureDir;
	mSkinned = isSkinned;

	if (mSkinned)
	{
		ReadBoneHierachy(scene->mRootNode);
		ReadBoneOffsets(scene);
		ReadAnimations(scene);
	}

	ProcessNode(
		scene->mRootNode,
		rootNode,
		scene);

	mFrameTransforms.clear();
	mBoneIndices.clear();
}

void Carol::AssimpModel::ProcessNode(
	aiNode* node,
	SceneNode* sceneNode,
	const aiScene* scene)
{
	for (int i = 0; i < node->mNumMeshes; ++i)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		sceneNode->Meshes.push_back(
			ProcessMesh(
				mesh,
				scene));
	}
	
	for (int i = 0; i < node->mNumChildren; ++i)
	{
		ProcessNode(
			node->mChildren[i],
			sceneNode,
			scene);
	}
}

Carol::Mesh* Carol::AssimpModel::ProcessMesh(
	aiMesh* mesh,
	const aiScene* scene)
{	
	wstring meshName = StringToWString(mesh->mName.C_Str());

	if (mMeshes.count(meshName) == 0)
	{
		vector<Vertex> vertices;
		vector<pair<wstring, vector<vector<Vertex>>>> skinnedVertices;
		vector<uint32_t> indices;

		ReadMeshVerticesAndIndices(vertices, indices, mesh);
		ReadMeshBones(vertices, mesh);

		for (auto& [name, clip] : mAnimationClips)
		{
			auto skinnedVerticesPair = make_pair(name, vector<vector<Vertex>>());
			GetSkinnedVertices(name, vertices, skinnedVerticesPair.second);
			skinnedVertices.emplace_back(std::move(skinnedVerticesPair));
		}

		mMeshes[meshName] = make_unique<Mesh>(
			vertices,
			skinnedVertices,
			indices,
			mSkinned & bool(mesh->mNumBones),
			false);

		ReadMeshMaterialAndTextures(
			mMeshes[meshName].get(),
			mesh,
			scene);
	}

	return mMeshes[meshName].get();
}

void Carol::AssimpModel::ReadBoneHierachy(aiNode* node)
{
	wstring nodeName = StringToWString(node->mName.C_Str());
	mBoneIndices[nodeName] = mBoneHierarchy.size();
	mBoneHierarchy.emplace_back(-1);

	if (node->mParent)
	{
		wstring parentName = StringToWString(node->mParent->mName.C_Str());
		mBoneHierarchy[mBoneIndices[nodeName]] = mBoneIndices[parentName];
	}

	for (int i = 0; i < node->mNumChildren; ++i)
	{
		ReadBoneHierachy(node->mChildren[i]);
	}
}

void Carol::AssimpModel::ReadBoneOffsets(const aiScene* scene)
{
	mBoneOffsets.resize(mBoneHierarchy.size());

	for (int i = 0; i < scene->mNumMeshes; ++i)
	{
		for (int j = 0; j < scene->mMeshes[i]->mNumBones; ++j)
		{
			auto* bone = scene->mMeshes[i]->mBones[j];
			uint32_t boneIdx = mBoneIndices[StringToWString(bone->mName.C_Str())];
			XMStoreFloat4x4(&mBoneOffsets[boneIdx], ai2XM(bone->mOffsetMatrix));
		}
	}
}

void Carol::AssimpModel::ReadMeshBones(vector<Vertex>& vertices, aiMesh* mesh)
{
	uint32_t boneIndex = 0;

	for (int i = 0; i < mesh->mNumBones; ++i)
	{
		wstring boneName = StringToWString(mesh->mBones[i]->mName.C_Str());
		boneIndex = mBoneIndices[boneName];

		for (int j = 0; j < mesh->mBones[i]->mNumWeights; ++j)
		{
			uint32_t vId = mesh->mBones[i]->mWeights[j].mVertexId;
			float weight = mesh->mBones[i]->mWeights[j].mWeight;
			InsertBoneWeightToVertex(vertices[vId], boneIndex, weight);
		}
	}
}

void Carol::AssimpModel::InsertBoneWeightToVertex(Vertex& vertex, uint32_t boneIndex, float boneWeight)
{
	if (vertex.Weights.x == 0.0f)
	{
		vertex.BoneIndices.x = boneIndex;
		vertex.Weights.x = boneWeight;
	}
	else if (vertex.Weights.y == 0.0f)
	{
		vertex.BoneIndices.y = boneIndex;
		vertex.Weights.y = boneWeight;
	}
	else if (vertex.Weights.z == 0.0f)
	{
		vertex.BoneIndices.z = boneIndex;
		vertex.Weights.z = boneWeight;
	}
	else
	{
		vertex.BoneIndices.w = boneIndex;
	}
}

void Carol::AssimpModel::ReadAnimations(const aiScene* scene)
{
	unordered_map<wstring, vector<vector<XMFLOAT4X4>>> criticalFrames;
	uint32_t boneCount = mBoneHierarchy.size();

	for (int i = 0; i < scene->mNumAnimations; ++i)
	{
		auto* animation = scene->mAnimations[i];

		unique_ptr<AnimationClip> animationClip = make_unique<AnimationClip>();
		vector<BoneAnimation>& boneAnimations=animationClip->BoneAnimations;
		boneAnimations.resize(boneCount);

		float ticksPerSecond = animation->mTicksPerSecond != 0 ? animation->mTicksPerSecond : 25.0f;
		float timePerTick = 1.0f / ticksPerSecond;

		for (int j = 0; j < animation->mNumChannels; ++j)
		{
			auto* nodeAnimation = animation->mChannels[j];
			wstring boneName = StringToWString(nodeAnimation->mNodeName.C_Str());
			auto& boneAnimation = boneAnimations[mBoneIndices[boneName]];

			auto& transKey = boneAnimation.TranslationKeyframes;
			auto& scaleKey = boneAnimation.ScaleKeyframes;
			auto& quatKey = boneAnimation.RotationQuatKeyframes;
			
			transKey.resize(nodeAnimation->mNumPositionKeys);
			for (int k = 0; k < nodeAnimation->mNumPositionKeys; ++k)
			{
				transKey[k].TimePos = nodeAnimation->mPositionKeys[k].mTime * timePerTick;
				transKey[k].Translation = 
				{ nodeAnimation->mPositionKeys[k].mValue.x,
				  nodeAnimation->mPositionKeys[k].mValue.y,
				  nodeAnimation->mPositionKeys[k].mValue.z };
			}

			scaleKey.resize(nodeAnimation->mNumScalingKeys);
			for (int k = 0; k < nodeAnimation->mNumScalingKeys; ++k)
			{
				scaleKey[k].TimePos = nodeAnimation->mScalingKeys[k].mTime * timePerTick;
				scaleKey[k].Scale = 
				{ nodeAnimation->mScalingKeys[k].mValue.x,
				  nodeAnimation->mScalingKeys[k].mValue.y,
				  nodeAnimation->mScalingKeys[k].mValue.z };
			}

			quatKey.resize(nodeAnimation->mNumRotationKeys);
			for (int k = 0; k < nodeAnimation->mNumRotationKeys; ++k)
			{
				quatKey[k].TimePos = nodeAnimation->mRotationKeys[k].mTime * timePerTick;
				quatKey[k].RotationQuat = 
				{ nodeAnimation->mRotationKeys[k].mValue.x,
				  nodeAnimation->mRotationKeys[k].mValue.y,
				  nodeAnimation->mRotationKeys[k].mValue.z,
				  nodeAnimation->mRotationKeys[k].mValue.w };
			}
			
		}

		animationClip->CalcClipStartTime();
		animationClip->CalcClipEndTime();
		wstring clipName = StringToWString(animation->mName.C_Str());
		mAnimationClips[clipName] = std::move(animationClip);
	}

	for (auto& [name, clip] : mAnimationClips)
	{
		vector<vector<XMFLOAT4X4>> frameTransforms;
		float t = clip->GetClipStartTime();

		while (t < clip->GetClipEndTime())
		{
			vector<XMFLOAT4X4> finalTransforms;
			GetFinalTransforms(name, t, finalTransforms);
			frameTransforms.emplace_back(std::move(finalTransforms));
			t += 0.1f;
		}

		mFrameTransforms[name] = std::move(frameTransforms);
	}
}

void Carol::AssimpModel::ReadMeshVerticesAndIndices(
	vector<Vertex>& vertices,
	vector<uint32_t>& indices,
	aiMesh* mesh)
{
	vertices.resize(mesh->mNumVertices);
	indices.resize(mesh->mNumFaces * 3);

	for (int i = 0; i < mesh->mNumVertices; ++i)
	{		
		auto& vPos = mesh->mVertices[i];
		vertices[i].Pos = { vPos.x,vPos.y,vPos.z };

		if (mesh->HasNormals())
		{
			auto&& vNormal = mesh->mNormals[i];
			vertices[i].Normal = { vNormal.x,vNormal.y,vNormal.z };
		}

		if (mesh->HasTangentsAndBitangents())
		{
			auto& vTangent = mesh->mTangents[i];
			vertices[i].Tangent = { vTangent.x,vTangent.y,vTangent.z };
		}

		if (mesh->HasTextureCoords(0))
		{
			auto& vTexC = mesh->mTextureCoords[0][i];
			vertices[i].TexC = { vTexC.x, vTexC.y };
		}

	}

	for (int i = 0; i < mesh->mNumFaces; ++i)
	{
		auto& face = mesh->mFaces[i];
		indices[i * 3 + 0] = face.mIndices[0];
		indices[i * 3 + 1] = face.mIndices[1];
		indices[i * 3 + 2] = face.mIndices[2];
	}

}

void Carol::AssimpModel::ReadMeshMaterialAndTextures(
	Mesh* mesh,
	aiMesh* aimesh,
	const aiScene* scene)
{
	auto* matData = scene->mMaterials[aimesh->mMaterialIndex];

	aiString diffusePath;
	aiString normalPath;
	aiString emissivePath;
	aiString metallicRoughnessPath;
	
	matData->GetTexture(aiTextureType_DIFFUSE, 0, &diffusePath);
	matData->GetTexture(aiTextureType_NORMALS, 0, &normalPath);
	matData->GetTexture(aiTextureType_EMISSIVE, 0, &emissivePath);
	matData->GetTexture(aiTextureType_METALNESS, 0, &metallicRoughnessPath);

	LoadTexture(mesh, diffusePath, aiTextureType_DIFFUSE);
	LoadTexture(mesh, normalPath, aiTextureType_NORMALS);
	LoadTexture(mesh, emissivePath, aiTextureType_EMISSIVE);
	LoadTexture(mesh, metallicRoughnessPath, aiTextureType_METALNESS);
}

void Carol::AssimpModel::LoadTexture(
	Mesh* mesh,
	aiString aiPath,
	aiTextureType type)
{
	wstring path = StringToWString(aiPath.C_Str());

	bool flag = path.size();
	if(flag)
	{
		int lastSeparator = path.find_last_of(L'\\');

		if (lastSeparator == -1)
		{
			lastSeparator = path.find_last_of(L'/');
		}

		path = mTexDir + L'\\' + path.substr(lastSeparator + 1);
		
		std::ifstream fstream(path);
		flag = fstream.good();
	}

	if (!flag)
	{
		switch (type)
		{
		case aiTextureType_DIFFUSE:
			path = L"texture\\default_diffuse_texture.png";
			break;
		case aiTextureType_NORMALS:
			path = L"texture\\default_normal_texture.png";
			break;
		case aiTextureType_EMISSIVE:
			path = L"texture\\default_emissive_texture.png";
			break;
		case aiTextureType_METALNESS:
			path = L"texture\\default_metallic_roughness_texture.png";
			break;
		}
	}
	
	switch (type)
	{
	case aiTextureType_DIFFUSE:
		mesh->SetDiffuseTextureIdx(gTextureManager->LoadTexture(path, false));
		break;
	case aiTextureType_NORMALS:
		mesh->SetNormalTextureIdx(gTextureManager->LoadTexture(path, false));
		break;
	case aiTextureType_EMISSIVE:
		mesh->SetEmissiveTextureIdx(gTextureManager->LoadTexture(path, false));
		break;
	case aiTextureType_METALNESS:
		mesh->SetMetallicRoughnessTextureIdx(gTextureManager->LoadTexture(path, false));
		break;
	}

	mTexturePath.push_back(path);
}

