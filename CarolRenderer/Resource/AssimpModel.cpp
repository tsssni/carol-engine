#include "AssimpModel.h"
#include "SkinnedData.h"
#include "../Utils/Common.h"
#include <assimp/postprocess.h>

void Carol::AssimpModel::LoadAssimpModel(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, wstring path, uint32_t matOffset, uint32_t& texOffset)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(WStringToString(path), aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace | aiProcess_FixInfacingNormals | aiProcess_JoinIdenticalVertices | aiProcess_ConvertToLeftHanded);
	
	assert(scene);
	
	uint32_t vertexCount = 0;
	uint32_t indexCount = 0;

	for (int i = 0; i < scene->mNumMeshes; ++i)
	{
		vertexCount += scene->mMeshes[i]->mNumVertices;
		indexCount += scene->mMeshes[i]->mNumFaces * 3;
	}
	
	vector<AssimpVertex> vertices;
	vector<uint32_t> indices;
	mMaterials.resize(scene->mNumMaterials);

	vertices.reserve(vertexCount);
	indices.reserve(indexCount);

	ProcessNode(device, cmdList, vertices, indices, matOffset, texOffset, scene->mRootNode, scene);
	LoadVerticesAndIndices(device, cmdList, vertices, indices);
}

void Carol::AssimpModel::ProcessNode(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, vector<AssimpVertex>& vertices, vector<uint32_t>& indices, uint32_t matOffset, uint32_t& texOffset, aiNode* node, const aiScene* scene)
{
	for (int i = 0; i < node->mNumMeshes; ++i)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		ProcessMesh(device, cmdList, vertices, indices, matOffset, texOffset, mesh, scene);
	}

	for (int i = 0; i < node->mNumChildren; ++i)
	{
		ProcessNode(device, cmdList, vertices, indices, matOffset, texOffset, node->mChildren[i], scene);
	}
}

void Carol::AssimpModel::ProcessMesh(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, vector<AssimpVertex>& vertices, vector<uint32_t>& indices, uint32_t matOffset, uint32_t& texOffset, aiMesh* mesh, const aiScene* scene)
{	
	static int index = -1;
	++index;

	InitSubmesh(StringToWString(mesh->mName.C_Str()) + std::to_wstring(index), 0, indices.size(), mesh->mNumFaces * 3, matOffset + mesh->mMaterialIndex);
	ReadMeshVerticesAndIndices(vertices, indices, mesh);
	ReadMeshMaterialAndTextures(matOffset, texOffset, mesh, scene);
}

void Carol::AssimpModel::ReadMeshVerticesAndIndices(vector<AssimpVertex>& vertices, vector<uint32_t>& indices, aiMesh* mesh)
{
	uint32_t vertexOffset = vertices.size();
	uint32_t indexOffset = indices.size() / 3;

	vertices.resize(vertexOffset + mesh->mNumVertices);
	indices.resize(indexOffset * 3 + mesh->mNumFaces * 3);

	for (int i = vertexOffset; i < vertexOffset + mesh->mNumVertices; ++i)
	{
		int j = i - vertexOffset;

		auto& vPos = mesh->mVertices[j];
		vertices[i].Pos = { vPos.x,vPos.y,vPos.z };
	
		if (mesh->HasNormals())
		{
			auto& vNormal = mesh->mNormals[j];
			vertices[i].Normal = { vNormal.x,vNormal.y,vNormal.z };
		}

		if (mesh->HasTangentsAndBitangents())
		{
			auto& vTangent = mesh->mTangents[j];
			vertices[i].Tangent = { vTangent.x,vTangent.y,vTangent.z };
		}

		if (mesh->HasTextureCoords(0))
		{
			auto& vTexC = mesh->mTextureCoords[0][j];
			vertices[i].TexC = { vTexC.x, vTexC.y };
		}

	}

	for (int i = indexOffset; i < indexOffset + mesh->mNumFaces; ++i)
	{
		auto face = mesh->mFaces[i - indexOffset];
		indices[i * 3 + 0] = face.mIndices[0] + indexOffset * 3;
		indices[i * 3 + 1] = face.mIndices[1] + indexOffset * 3;
		indices[i * 3 + 2] = face.mIndices[2] + indexOffset * 3;
	}

}

void Carol::AssimpModel::ReadMeshMaterialAndTextures(uint32_t matOffset, uint32_t& texOffset, aiMesh* mesh, const aiScene* scene)
{
	if (mesh->mMaterialIndex < 0)
	{
		return;
	}

	auto* matData = scene->mMaterials[mesh->mMaterialIndex];
	auto& mat = mMaterials[mesh->mMaterialIndex];
	mat.MatTBIndex = matOffset + mesh->mMaterialIndex;

	aiColor4D color4D;

	aiGetMaterialColor(matData, AI_MATKEY_COLOR_EMISSIVE, &color4D);
	mat.Emissive = { color4D.r,color4D.g,color4D.b };

	aiGetMaterialColor(matData, AI_MATKEY_COLOR_AMBIENT, &color4D);
	mat.Ambient = { color4D.r,color4D.g,color4D.b };

	aiGetMaterialColor(matData, AI_MATKEY_COLOR_DIFFUSE, &color4D);
	mat.Diffuse = { color4D.r,color4D.g,color4D.b };

	aiGetMaterialColor(matData, AI_MATKEY_COLOR_SPECULAR, &color4D);
	mat.Specular = { color4D.r,color4D.g,color4D.b };

	aiGetMaterialColor(matData, AI_MATKEY_COLOR_REFLECTIVE, &color4D);
	mat.Reflection = { color4D.r,color4D.g,color4D.b };

	aiGetMaterialFloat(matData, AI_MATKEY_SHININESS, &mat.Shininess);

	ReadTexture(texOffset, matData, mat, aiTextureType_DIFFUSE);
	ReadTexture(texOffset, matData, mat, aiTextureType_NORMALS);
}

void Carol::AssimpModel::ReadTexture(uint32_t& texOffset, aiMaterial* matData, AssimpMaterial& mat, aiTextureType type)
{
	aiString path;
	wstring qPath;

	int* mapPtr = nullptr;
	switch (type)
	{
	case aiTextureType_DIFFUSE:
		mapPtr=&mat.DiffuseMapIndex; 
		matData->GetTexture(aiTextureType_DIFFUSE, 0, &path);
		break;
	case aiTextureType_NORMALS:
		mapPtr=&mat.NormalMapIndex;
		matData->GetTexture(aiTextureType_NORMALS, 0, &path);
		break;
	}

	if (path.length == 0)
	{
		return;
	}

	wstring fileName = StringToWString(path.C_Str());
	int lastSeparator = fileName.find_last_of(L'\\');
	if (lastSeparator != -1)
	{
		fileName = fileName.substr(lastSeparator); 
	}
	
	if (mTextureIndexMap.count(fileName) != 0)
	{
		*mapPtr = mTextureIndexMap[fileName];
	}
	else
	{
		*mapPtr = texOffset;
		mTextureIndexMap[fileName] = texOffset;
		++texOffset;
	}

}

vector<Carol::AssimpMaterial>& Carol::AssimpModel::GetMaterials()
{
	return mMaterials;
}

unordered_map<wstring, uint32_t>& Carol::AssimpModel::GetTextures()
{
	return mTextureIndexMap;
}

unique_ptr<Carol::AssimpModel> Carol::AssimpModel::GetFlatGround(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, uint32_t matOffset, uint32_t& texOffset)
{
	auto flatGround = std::make_unique<AssimpModel>();

	vector<AssimpVertex> vertices(4);
	vector<uint32_t> indices = { 0,1,2,0,2,3 };
	vertices[0].Pos = { -200,0,-200 }; vertices[0].Normal = { 0,1,0 };
	vertices[1].Pos = { -200,0,200 }; vertices[1].Normal = { 0,1,0 };
	vertices[2].Pos = { 200,0,200 }; vertices[2].Normal = { 0,1,0 };
	vertices[3].Pos = { 200,0,-200 }; vertices[3].Normal = { 0,1,0 };

	flatGround->LoadVerticesAndIndices(device, cmdList, vertices, indices);

	flatGround->mMaterials.push_back({});
	flatGround->mMaterials[0].DiffuseMapIndex = texOffset;
	flatGround->mMaterials[0].MatTBIndex = matOffset;
	flatGround->mTextureIndexMap[L"FlatGround.png"] = texOffset++;

	flatGround->InitSubmesh(L"FlatGround", 0, 0, 6, matOffset);

	return flatGround;
}

void Carol::SkinnedAssimpModel::LoadSkinnedAssimpModel(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, wstring path, uint32_t matOffset, uint32_t& texOffset)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(WStringToString(path), aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices | aiProcess_FixInfacingNormals | aiProcess_LimitBoneWeights | aiProcess_ConvertToLeftHanded);
	
	assert(scene);

	uint32_t vertexCount = 0;
	uint32_t indexCount = 0;

	for (int i = 0; i < scene->mNumMeshes; ++i)
	{
		vertexCount += scene->mMeshes[i]->mNumVertices;
		indexCount += scene->mMeshes[i]->mNumFaces * 3;
	}
	
	vector<SkinnedAssimpVertex> vertices;
	vector<uint32_t> indices;
	mMaterials.resize(scene->mNumMaterials);

	vertices.reserve(vertexCount);
	indices.reserve(indexCount);
	
	uint32_t boneCount = 0;
	ReadBones(scene->mRootNode, scene);

	mBoneHierachy.resize(mBoneIndices.size());
	ReadBoneHierachy(scene->mRootNode, boneCount);
	
	mBoneOffsets.resize(boneCount);
	for (int i = 0; i < mBoneOffsets.size(); ++i)
	{
		mBoneOffsets[i] =
		{
			1.0f,0.0f,0.0f,0.0f,
			0.0f,1.0f,0.0f,0.0f,
			0.0f,0.0f,1.0f,0.0f,
			0.0f,0.0f,0.0f,1.0f
		};
	}
	
	mBoneInited.resize(boneCount);
	ProcessNode(vertices, indices, matOffset, texOffset, scene->mRootNode, scene);
	
	LoadVerticesAndIndices(device, cmdList, vertices, indices);
	ReadAnimation(scene);
	mSkinnedData.Set(mBoneHierachy, mBoneOffsets, mAnimationClips);
}

Carol::SkinnedData& Carol::SkinnedAssimpModel::GetSkinnedData()
{
	return mSkinnedData;
}

void Carol::SkinnedAssimpModel::ReadBones(aiNode* node, const aiScene* scene)
{
	for (int i = 0; i < node->mNumMeshes; ++i)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		
		for (int j = 0; j < mesh->mNumBones; ++j)
		{
			wstring boneName = StringToWString(mesh->mBones[j]->mName.C_Str());
			mBoneIndices.insert({ boneName, -1 });
		}
	}

	for (int i = 0; i < node->mNumChildren; ++i)
	{
		ReadBones(node->mChildren[i], scene);
	}
}

void Carol::SkinnedAssimpModel::ReadBoneHierachy(aiNode* node, uint32_t& boneCount)
{
	wstring nodeName = StringToWString(node->mName.C_Str());

	if (node->mParent)
	{
		wstring parentName = StringToWString(node->mParent->mName.C_Str());

		if (mBoneIndices.count(nodeName) != 0)
		{
			mBoneIndices[nodeName] = boneCount++;

			if (mBoneIndices.count(parentName) == 0)
			{
				mBoneHierachy[mBoneIndices[nodeName]] = -1;
			}
			else
			{
				mBoneHierachy[mBoneIndices[nodeName]] = mBoneIndices[parentName];
			}
		}
	}

	for (int i = 0; i < node->mNumChildren; ++i)
	{
		auto name = node->mChildren[i]->mName;
		ReadBoneHierachy(node->mChildren[i], boneCount);
	}
}

void Carol::SkinnedAssimpModel::ProcessNode(vector<SkinnedAssimpVertex>& vertices, vector<uint32_t>& indices, uint32_t matOffset, uint32_t& texOffset, aiNode* node, const aiScene* scene)
{
	for (int i = 0; i < node->mNumMeshes; ++i)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		ProcessMesh(vertices, indices, matOffset, texOffset, mesh, scene);
	}

	for (int i = 0; i < node->mNumChildren; ++i)
	{
		ProcessNode(vertices, indices, matOffset, texOffset, node->mChildren[i], scene);
	}
}

void Carol::SkinnedAssimpModel::ProcessMesh(vector<SkinnedAssimpVertex>& vertices, vector<uint32_t>& indices, uint32_t matOffset, uint32_t& texOffset, aiMesh* mesh, const aiScene* scene)
{
	static int index = -1;
	++index;
	uint32_t vertexOffset = vertices.size();
	
	InitSubmesh(StringToWString(mesh->mName.C_Str()) + std::to_wstring(index), 0, indices.size(), mesh->mNumFaces * 3, matOffset + mesh->mMaterialIndex);
	ReadMeshVerticesAndIndices(vertices, indices, mesh);
	ReadMeshMaterialAndTextures(matOffset, texOffset, mesh, scene);
	ReadMeshBones(vertices, vertexOffset, mesh);
}

void Carol::SkinnedAssimpModel::ReadMeshVerticesAndIndices(vector<SkinnedAssimpVertex>& vertices, vector<uint32_t>& indices, aiMesh* mesh)
{
	uint32_t vertexOffset = vertices.size();
	uint32_t indexOffset = indices.size() / 3;

	vertices.resize(vertexOffset + mesh->mNumVertices);
	indices.resize(indexOffset * 3 + mesh->mNumFaces * 3);

	for (int i = vertexOffset; i < vertexOffset + mesh->mNumVertices; ++i)
	{
		int j = i - vertexOffset;

		auto& vPos = mesh->mVertices[j];
		vertices[i].Pos = { vPos.x,vPos.y,vPos.z };

		if (mesh->HasNormals())
		{
			auto& vNormal = mesh->mNormals[j];
			vertices[i].Normal = { vNormal.x,vNormal.y,vNormal.z };
		}

		if (mesh->HasTangentsAndBitangents())
		{
			auto& vTangent = mesh->mTangents[j];
			vertices[i].Tangent = { vTangent.x,vTangent.y,vTangent.z };
		}

		if (mesh->HasTextureCoords(0))
		{
			auto& vTexC = mesh->mTextureCoords[0][j];
			vertices[i].TexC = { vTexC.x, vTexC.y };
		}

	}

	for (int i = indexOffset; i < indexOffset + mesh->mNumFaces; ++i)
	{
		auto face = mesh->mFaces[i - indexOffset];
		indices[i * 3 + 0] = face.mIndices[0] + indexOffset * 3;
		indices[i * 3 + 1] = face.mIndices[1] + indexOffset * 3;
		indices[i * 3 + 2] = face.mIndices[2] + indexOffset * 3;
	}

}

void Carol::SkinnedAssimpModel::ReadMeshBones(vector<SkinnedAssimpVertex>& vertices, uint32_t vertexOffset, aiMesh* mesh)
{
	uint32_t boneIndex = 0;

	for (int i = 0; i < mesh->mNumBones; ++i)
	{
		wstring boneName = StringToWString(mesh->mBones[i]->mName.C_Str());

		boneIndex = mBoneIndices[boneName];
		if (!mBoneInited[boneIndex])
		{
			mBoneInited[boneIndex] = true;
			
			auto boneOffset = mesh->mBones[i]->mOffsetMatrix;
			mBoneOffsets[boneIndex] =
			{
				boneOffset.a1, boneOffset.b1, boneOffset.c1, boneOffset.d1,
				boneOffset.a2, boneOffset.b2, boneOffset.c2, boneOffset.d2,
				boneOffset.a3, boneOffset.b3, boneOffset.c3, boneOffset.d3,
				boneOffset.a4, boneOffset.b4, boneOffset.c4, boneOffset.d4
			};
		}

		for (int j = 0; j < mesh->mBones[i]->mNumWeights; ++j)
		{
			uint32_t vId = mesh->mBones[i]->mWeights[j].mVertexId;
			float weight = mesh->mBones[i]->mWeights[j].mWeight;
			InsertBoneWeightToVertex(vertices[vertexOffset + vId], boneIndex, weight);
		}
	}
}

void Carol::SkinnedAssimpModel::InsertBoneWeightToVertex(SkinnedAssimpVertex& vertex, uint32_t boneIndex, float boneWeight)
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

void Carol::SkinnedAssimpModel::ReadAnimation(const aiScene* scene)
{
	for (int i = 0; i < scene->mNumAnimations; ++i)
	{
		auto* animation = scene->mAnimations[i];

		AnimationClip animationClip;
		vector<BoneAnimation>& boneAnimations=animationClip.BoneAnimations;
		boneAnimations.resize(mBoneIndices.size());

		float ticksPerSecond = animation->mTicksPerSecond == 0 ? animation->mTicksPerSecond : 25.0f;
		float timePerTick = 1.0f / ticksPerSecond;

		for (int j = 0; j < animation->mNumChannels; ++j)
		{
			auto* nodeAnimation = animation->mChannels[j];
			wstring boneName = StringToWString(nodeAnimation->mNodeName.C_Str());
			if (mBoneIndices[boneName] == -1)
				continue;
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

		mAnimationClips[StringToWString(animation->mName.C_Str())] = std::move(animationClip);
	}
}
