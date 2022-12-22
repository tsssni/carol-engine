#include <scene/assimp.h>
#include <render/global_resources.h>
#include <render/mesh.h>
#include <dx12/heap.h>
#include <scene/skinned_data.h>
#include <scene/timer.h>
#include <scene/texture.h>
#include <utils/common.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <memory>
#include <algorithm>
#include <fstream>

#define aiProcess_Static aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_GenBoundingBoxes | aiProcess_JoinIdenticalVertices | aiProcess_FixInfacingNormals | aiProcess_PreTransformVertices | aiProcess_ConvertToLeftHanded
#define aiProcess_Skinned aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_GenBoundingBoxes | aiProcess_JoinIdenticalVertices | aiProcess_FixInfacingNormals | aiProcess_LimitBoneWeights | aiProcess_ConvertToLeftHanded

namespace Carol {
	using std::vector;
	using std::unique_ptr;
	using std::make_unique;
	using namespace DirectX;
}

Carol::AssimpModel::AssimpModel(GlobalResources* globalResources)
	:mGlobalResources(globalResources)
{
}

Carol::AssimpModel::AssimpModel(GlobalResources* globalResources, wstring path, wstring textureDir, bool isSkinned, bool isTransparent)
{
	Assimp::Importer mImporter;
	const aiScene* scene = mImporter.ReadFile(WStringToString(path), isSkinned ? aiProcess_Skinned : aiProcess_Static);
	
	if (!scene)
	{
		mInitSucceeded = false;
		return;
	}
	else
	{
		mInitSucceeded = true;
	}

	mGlobalResources = globalResources;
	mTextureDir = textureDir;
	mSkinned = isSkinned;
	mTransparent = isTransparent;
	
	uint32_t vertexCount = 0;
	uint32_t indexCount = 0;

	for (int i = 0; i < scene->mNumMeshes; ++i)
	{
		vertexCount += scene->mMeshes[i]->mNumVertices;
		indexCount += scene->mMeshes[i]->mNumFaces * 3;
	}

	mVertices.reserve(vertexCount);
	mIndices.reserve(indexCount);

	LoadAssimpSkinnedData(scene);
	ProcessNode(scene->mRootNode, scene);
	LoadVerticesAndIndices(mGlobalResources->CommandList, mGlobalResources->DefaultBuffersHeap, mGlobalResources->UploadBuffersHeap, mVertices, mIndices);

	mVertices.clear();
	mVertices.shrink_to_fit();
	mIndices.clear();
	mIndices.shrink_to_fit();
}

void Carol::AssimpModel::InitSkinnedCBHeap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
	if (!SkinnedCBHeap)
	{
		SkinnedCBHeap = make_unique<CircularHeap>(device, cmdList, true, 32, sizeof(SkinnedConstants));
	}
}

Carol::CircularHeap* Carol::AssimpModel::GetSkinnedCBHeap()
{
	return SkinnedCBHeap.get();
}

bool Carol::AssimpModel::InitSucceeded()
{
	return mInitSucceeded;
}

bool Carol::AssimpModel::IsSkinned()
{
	return mSkinned;
}

void Carol::AssimpModel::LoadFlatGround()
{
	XMFLOAT3 pos[4] =
	{
		{-50.0f,0.0f,-50.0f},
		{-50.0f,0.0f,50.0f},
		{50.0f,0.0f,50.0f},
		{50.0f,0.0f,-50.0f}
	};

	XMFLOAT2 texC[4] =
	{
		{0.0f,50.0f},
		{0.0f,0.0f},
		{50.0f,0.0f},
		{50.0f,50.0f}
	};

	XMFLOAT3 normal = { 0.0f,1.0f,0.0f };
	XMFLOAT3 tangent = { 1.0f,0.0f,0.0f };

	vector<AssimpVertex> mVertices(4);
	for (int i = 0; i < 4; ++i)
	{
		mVertices[i] = { pos[i],normal,tangent,texC[i] };
	}
	vector<uint16_t> mIndices = { 0,1,2,0,2,3 };
	LoadVerticesAndIndices(mGlobalResources->CommandList, mGlobalResources->DefaultBuffersHeap, mGlobalResources->UploadBuffersHeap, mVertices, mIndices);

	mMeshes[L"Ground"] = make_unique<MeshPass>(mGlobalResources, false, nullptr, false, 0, 0, 6, mVertexBufferView, mIndexBufferView);
	mMeshes[L"Ground"]->LoadDiffuseMap(L"texture\\tile.dds");
	mMeshes[L"Ground"]->LoadNormalMap(L"texture\\tile_nmap.dds");
	mMeshes[L"Ground"]->SetBoundingBox({ -50.0f,-0.1f,-50.0f }, { 50.0f,0.1f,50.0f });

	SetWorld(XMMatrixIdentity());
}

void Carol::AssimpModel::LoadSkyBox()
{
	XMFLOAT3 pos[8] =
	{
		{-200.0f,-200.0f,200.0f},
		{-200.0f,200.0f,200.0f},
		{200.0f,200.0f,200.0f},
		{200.0f,-200.0f,200.0f},
		{-200.0f,-200.0f,-200.0f},
		{-200.0f,200.0f,-200.0f},
		{200.0f,200.0f,-200.0f},
		{200.0f,-200.0f,-200.0f}
	};

	vector<AssimpVertex> mVertices(8);
	for (int i = 0; i < 8; ++i)
	{
		mVertices[i] = { pos[i] };
	}

	vector<uint16_t> mIndices = { 0,1,2,0,2,3,4,5,1,4,1,0,7,6,5,7,5,4,3,2,6,3,6,7,1,5,6,1,6,2,4,0,3,4,3,7 };
	LoadVerticesAndIndices(mGlobalResources->CommandList, mGlobalResources->DefaultBuffersHeap, mGlobalResources->UploadBuffersHeap, mVertices, mIndices);

	mMeshes[L"SkyBox"] = make_unique<MeshPass>(mGlobalResources, false, nullptr, false, 0, 0, 36, mVertexBufferView, mIndexBufferView);
	mGlobalResources->SkyBoxMesh = mMeshes[L"SkyBox"].get();
}

void Carol::AssimpModel::SetWorld(XMMATRIX world)
{
	XMStoreFloat4x4(&mWorld, world);

	for (auto& meshMapPair : mMeshes)
	{
		meshMapPair.second->SetWorld(world);
	}
}

void Carol::AssimpModel::SetAnimation(wstring clipName)
{
	mClipName = clipName;
}

Carol::vector<wstring> Carol::AssimpModel::GetAnimations()
{
	vector<wstring> animations;

	for (auto& aniMapPair : mAnimations)
	{
		animations.push_back(aniMapPair.first);
	}

	return animations;
}

void Carol::AssimpModel::UpdateAnimation()
{
	if (mSkinned)
	{
		mTimePos += mGlobalResources->Timer->DeltaTime();

		if (mAnimations.count(mClipName) == 0)
		{
			return;
		}

		if (mTimePos > mAnimations[mClipName]->GetClipEndTime())
		{
			mTimePos = 0;
		}

		vector<XMFLOAT4X4> toParentTransforms(mSkinnedCount);
		mAnimations[mClipName]->Interpolate(mTimePos, toParentTransforms);

		vector<XMFLOAT4X4> toRootTransforms(mSkinnedCount);
		bool flag = true;

		for (int i = 0; i < mSkinnedCount; ++i)
		{
			XMMATRIX toParent = mBoneMark[i] ? XMLoadFloat4x4(&toParentTransforms[i]) : XMMatrixIdentity();
			XMMATRIX parentToRoot = mBoneHierarchy[i] != -1 ? XMLoadFloat4x4(&toRootTransforms[mBoneHierarchy[i]]) : XMMatrixIdentity();

			XMMATRIX toRoot = toParent * parentToRoot;
			XMStoreFloat4x4(&toRootTransforms[i], toRoot);

			if (flag && mBoneMark[i])
			{
				for (auto& meshMapPair : mMeshes)
				{
					meshMapPair.second->TransformBoundingBox(XMMatrixScaling(2.f,2.f,2.f) * toRoot * XMLoadFloat4x4(&mWorld));
				}

				flag = false;
			}
		}

		for (int i = 0; i < mSkinnedCount; ++i)
		{
			XMMATRIX offset = XMLoadFloat4x4(&mBoneOffsets[i]);
			XMMATRIX toRoot = XMLoadFloat4x4(&toRootTransforms[i]);

			XMStoreFloat4x4(&mFinalTransforms[i], XMMatrixTranspose(offset * toRoot));
		}

		std::copy(std::begin(mSkinnedConstants->FinalTransforms), std::end(mSkinnedConstants->FinalTransforms), mSkinnedConstants->HistoryFinalTransforms);
		std::copy(std::begin(mFinalTransforms), std::end(mFinalTransforms), mSkinnedConstants->FinalTransforms);

		SkinnedCBHeap->DeleteResource(mSkinnedCBAllocInfo.get());
		SkinnedCBHeap->CreateResource(nullptr, nullptr, mSkinnedCBAllocInfo.get());
		SkinnedCBHeap->CopyData(mSkinnedCBAllocInfo.get(), mSkinnedConstants.get());
	}
}

void Carol::AssimpModel::ProcessNode(aiNode* node, const aiScene* scene)
{
	for (int i = 0; i < node->mNumMeshes; ++i)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		ProcessMesh(mesh, scene);
	}

	for (int i = 0; i < node->mNumChildren; ++i)
	{
		ProcessNode(node->mChildren[i], scene);
	}
}

void Carol::AssimpModel::ProcessMesh(aiMesh* mesh, const aiScene* scene)
{	
	uint32_t initVertexOffset = mVertices.size();
	wstring meshName = StringToWString(mesh->mName.C_Str());
	static uint32_t count = 0;

	if (mMeshes.count(meshName))
	{
		meshName += std::to_wstring(count);
		++count;
	}

	mMeshes[meshName] = make_unique<MeshPass>(mGlobalResources, mSkinned, mSkinnedCBAllocInfo.get(), mTransparent, 0, mIndices.size(), mesh->mNumFaces * 3, mVertexBufferView, mIndexBufferView);
	mMeshes[meshName]->SetBoundingBox(&mesh->mAABB);

	ReadMeshVerticesAndIndices(mesh);
	ReadMeshMaterialAndTextures(*mMeshes[meshName], mesh, scene);
	ReadMeshBones(initVertexOffset, mesh);
}

void Carol::AssimpModel::LoadAssimpSkinnedData(const aiScene* scene)
{
	mSkinnedConstants = make_unique<SkinnedConstants>();
	mSkinnedCBAllocInfo = make_unique<HeapAllocInfo>();

	ReadBones(scene->mRootNode, scene);
	InitBoneData(scene);

	ReadBoneHierarchy(scene->mRootNode);
	ReadBoneOffsets(scene->mRootNode, scene);
	ReadSkinnedNodeTransforms(scene->mRootNode);
	ReadAnimations(scene);
}

void Carol::AssimpModel::ReadBones(aiNode* node, const aiScene* scene)
{
	for (int i = 0; i < node->mNumMeshes; ++i)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		
		for (int j = 0; j < mesh->mNumBones; ++j)
		{
			wstring boneName = StringToWString(mesh->mBones[j]->mName.C_Str());
			MarkSkinnedNodes(scene->mRootNode, node, boneName);
		}
	}

	for (int i = 0; i < node->mNumChildren; ++i)
	{
		ReadBones(node->mChildren[i], scene);
	}

}

void Carol::AssimpModel::MarkSkinnedNodes(aiNode* node, aiNode* meshNode, wstring boneName)
{
	wstring nodeName = StringToWString(node->mName.C_Str());

	if (nodeName == boneName)
	{
		auto* itr = node;

		while (itr && itr != meshNode && itr != meshNode->mParent)
		{
			wstring itrName = StringToWString(itr->mName.C_Str());
			mSkinnedMark[itrName] = true;
			itr = itr->mParent;
		}
	}
	else
	{
		for (int i = 0; i < node->mNumChildren; ++i)
		{
			MarkSkinnedNodes(node->mChildren[i], meshNode, boneName);
		}
	}
}

void Carol::AssimpModel::InitBoneData(const aiScene* scene)
{
	mFinalTransforms.resize(mSkinnedMark.size());
	mSkinnedNodeTransforms.resize(mSkinnedMark.size());
	mBoneMark.resize(mSkinnedMark.size(), 0);
	mBoneHierarchy.resize(mSkinnedMark.size(), -1);
	mBoneOffsets.resize(mSkinnedMark.size());

	XMMATRIX identity = XMMatrixIdentity();

	for (int i = 0; i < mBoneOffsets.size(); ++i)
	{
		XMStoreFloat4x4(&mBoneOffsets[i], identity);
	}
}

void Carol::AssimpModel::ReadBoneHierarchy(aiNode* node)
{
	wstring nodeName = StringToWString(node->mName.C_Str());
	
	if (mSkinnedMark.count(nodeName))
	{
		mBoneHierarchy[mSkinnedCount] = -1;
		mBoneIndices[nodeName] = mSkinnedCount;

		if (node->mParent)
		{
			wstring parentName = StringToWString(node->mParent->mName.C_Str());
			if (mSkinnedMark.count(parentName))
			{
				mBoneHierarchy[mSkinnedCount] = mBoneIndices[parentName];
			}
		}

		++mSkinnedCount;
	}

	for (int i = 0; i < node->mNumChildren; ++i)
	{
		ReadBoneHierarchy(node->mChildren[i]);
	}
}

void Carol::AssimpModel::ReadBoneOffsets(aiNode* node, const aiScene* scene)
{
	for (int i = 0; i < node->mNumMeshes; ++i)
	{
		auto* mesh = scene->mMeshes[node->mMeshes[i]];
		
		for (int j = 0; j < mesh->mNumBones; ++j)
		{
			auto* bone = mesh->mBones[j];
			wstring boneName = StringToWString(bone->mName.C_Str());

			XMMATRIX offset = aiMatrix4x4ToXM(bone->mOffsetMatrix);
			XMStoreFloat4x4(&mBoneOffsets[mBoneIndices[boneName]], offset);
		}
	}

	for (int i = 0; i < node->mNumChildren; ++i)
	{
		ReadBoneOffsets(node->mChildren[i], scene);
	}
}

void Carol::AssimpModel::ReadSkinnedNodeTransforms(aiNode* node)
{
	wstring nodeName = StringToWString(node->mName.C_Str());

	if (mSkinnedMark.count(nodeName))
	{
		XMMATRIX transform = aiMatrix4x4ToXM(node->mTransformation);
		XMStoreFloat4x4(&mSkinnedNodeTransforms[mBoneIndices[nodeName]], transform);
	}

	for (int i = 0; i < node->mNumChildren; ++i)
	{
		ReadSkinnedNodeTransforms(node->mChildren[i]);
	}
}

void Carol::AssimpModel::ReadMeshBones(uint32_t vertexOffset, aiMesh* mesh)
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
			InsertBoneWeightToVertex(mVertices[vertexOffset + vId], boneIndex, weight);
		}
	}
}

void Carol::AssimpModel::InsertBoneWeightToVertex(AssimpVertex& vertex, uint32_t boneIndex, float boneWeight)
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
	for (int i = 0; i < scene->mNumAnimations; ++i)
	{
		auto* animation = scene->mAnimations[i];

		unique_ptr<AnimationClip> animationClip = make_unique<AnimationClip>();
		vector<BoneAnimation>& boneAnimations=animationClip->BoneAnimations;
		boneAnimations.resize(mBoneIndices.size());

		float ticksPerSecond = animation->mTicksPerSecond != 0 ? animation->mTicksPerSecond : 25.0f;
		float timePerTick = 1.0f / ticksPerSecond;

		for (int j = 0; j < animation->mNumChannels; ++j)
		{
			auto* nodeAnimation = animation->mChannels[j];
			wstring boneName = StringToWString(nodeAnimation->mNodeName.C_Str());

			if (mBoneIndices.count(boneName) == 0)
			{
				continue;
			}

			mBoneMark[mBoneIndices[boneName]] = 1;
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
	
		mAnimations[StringToWString(animation->mName.C_Str())] = std::move(animationClip);
	}
}

Carol::XMMATRIX Carol::AssimpModel::aiMatrix4x4ToXM(aiMatrix4x4 aiM)
{
	XMFLOAT4X4 M =
	{
		aiM.a1,aiM.b1,aiM.c1,aiM.d1,
		aiM.a2,aiM.b2,aiM.c2,aiM.d2,
		aiM.a3,aiM.b3,aiM.c3,aiM.d3,
		aiM.a4,aiM.b4,aiM.c4,aiM.d4
	};

	return XMLoadFloat4x4(&M);
}

aiMatrix4x4 Carol::AssimpModel::XMToaiMatrix4x4(XMMATRIX xm)
{
	XMFLOAT4X4 xm4x4;
	XMStoreFloat4x4(&xm4x4, xm);

	aiMatrix4x4 aiM =
	{
		xm4x4._11,xm4x4._21,xm4x4._31,xm4x4._41,
		xm4x4._12,xm4x4._22,xm4x4._32,xm4x4._42,
		xm4x4._13,xm4x4._23,xm4x4._33,xm4x4._43,
		xm4x4._14,xm4x4._24,xm4x4._34,xm4x4._44
	};
	
	return aiM;
}

void Carol::AssimpModel::ReadMeshVerticesAndIndices(aiMesh* mesh)
{
	uint32_t vertexOffset = mVertices.size();
	uint32_t indexOffset = mIndices.size() / 3;

	mVertices.resize(vertexOffset + mesh->mNumVertices);
	mIndices.resize(indexOffset * 3 + mesh->mNumFaces * 3);

	for (int i = vertexOffset; i < vertexOffset + mesh->mNumVertices; ++i)
	{
		int j = i - vertexOffset;
		
		auto vPos = mesh->mVertices[j];
		mVertices[i].Pos = { vPos.x,vPos.y,vPos.z };

		if (mesh->HasNormals())
		{
			auto vNormal = mesh->mNormals[j];
			mVertices[i].Normal = { vNormal.x,vNormal.y,vNormal.z };
		}

		if (mesh->HasTangentsAndBitangents())
		{
			auto vTangent = mesh->mTangents[j];
			mVertices[i].Tangent = { vTangent.x,vTangent.y,vTangent.z };
		}

		if (mesh->HasTextureCoords(0))
		{
			auto vTexC = mesh->mTextureCoords[0][j];
			mVertices[i].TexC = { vTexC.x, vTexC.y };
		}

	}

	for (int i = indexOffset; i < indexOffset + mesh->mNumFaces; ++i)
	{
		auto face = mesh->mFaces[i - indexOffset];
		mIndices[i * 3 + 0] = vertexOffset + face.mIndices[0];
		mIndices[i * 3 + 1] = vertexOffset + face.mIndices[1];
		mIndices[i * 3 + 2] = vertexOffset + face.mIndices[2];
	}

}

void Carol::AssimpModel::ReadMeshMaterialAndTextures(MeshPass& submesh, aiMesh* mesh, const aiScene* scene)
{
	auto& meshConstants = submesh.GetMeshConstants();
	auto* matData = scene->mMaterials[mesh->mMaterialIndex];
	aiColor4D color4D;

	aiGetMaterialColor(matData, AI_MATKEY_COLOR_REFLECTIVE, &color4D);
	meshConstants.FresnelR0 = { color4D.r,color4D.g,color4D.b };

	aiGetMaterialFloat(matData, AI_MATKEY_SHININESS, &meshConstants.Roughness);
	meshConstants.Roughness = 1.0f - meshConstants.Roughness;

	ReadTexture(submesh, matData);
}

void Carol::AssimpModel::ReadTexture(MeshPass& mesh, aiMaterial* matData)
{
	aiString diffusePath;
	aiString normalPath;
	
	matData->GetTexture(aiTextureType_DIFFUSE, 0, &diffusePath);
	matData->GetTexture(aiTextureType_NORMALS, 0, &normalPath);

	LoadTexture(mesh, diffusePath, aiTextureType_DIFFUSE);
	LoadTexture(mesh, normalPath, aiTextureType_NORMALS);
}

void Carol::AssimpModel::LoadTexture(MeshPass& mesh, aiString aiPath, aiTextureType type)
{
	wstring path = StringToWString(aiPath.C_Str());
	const static wstring defaultDiffuseMapPath = L"texture\\default_diffuse_map.png";
	const static wstring defaultNormalMapPath = L"texture\\default_normal_map.png";

	bool flag = path.size();
	if(flag)
	{
		int lastSeparator = path.find_last_of(L'\\');
		path = mTextureDir + L'\\' + path.substr(lastSeparator + 1);
		
		std::ifstream fstream(path);
		flag = fstream.good();
	}

	if (!flag)
	{
		switch (type)
		{
		case aiTextureType_DIFFUSE:
			path = L"texture\\default_diffuse_map.png";
			break;
		case aiTextureType_NORMALS:
			path = L"texture\\default_normal_map.png";
			break;
		}
	}
	
	switch (type)
	{
	case aiTextureType_DIFFUSE:
		mesh.LoadDiffuseMap(path);
		break;
	case aiTextureType_NORMALS:
		mesh.LoadNormalMap(path);
		break;
	}
}