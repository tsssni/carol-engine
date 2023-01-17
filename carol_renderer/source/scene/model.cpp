#include <scene/model.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/descriptor.h>
#include <scene/skinned_data.h>
#include <scene/timer.h>
#include <scene/texture.h>
#include <scene/material.h>
#include <utils/common.h>
#include <global.h>
#include <cmath>
#include <ranges>
#include <algorithm>

namespace Carol
{
	using std::vector;
	using std::span;
	using std::unique_ptr;
	using std::make_unique;
	using std::unordered_map;
	using std::wstring;
	using std::wstring_view;
	using std::pair;
	using std::make_pair;
	using namespace DirectX;

}

Carol::Model::Model()
	:mSkinnedConstants(make_unique<SkinnedConstants>())
{
	
}

Carol::Model::~Model()
{
}

void Carol::Model::ReleaseIntermediateBuffers()
{
	for (auto& meshMapPair : mMeshes)
	{
		auto& mesh = meshMapPair.second;
		mesh->ReleaseIntermediateBuffer();
	}
}

Carol::Mesh* Carol::Model::GetMesh(std::wstring meshName)
{
	return mMeshes[meshName].get();
}

const Carol::unordered_map<Carol::wstring, Carol::unique_ptr<Carol::Mesh>>& Carol::Model::GetMeshes()
{
	return mMeshes;
}

Carol::vector<Carol::wstring_view> Carol::Model::GetAnimationClips()
{
	vector<wstring_view> animations;

	for (auto& [name, animation] : mAnimationClips)
	{
		animations.push_back(wstring_view(name.c_str(),name.size()));
	}

	return animations;
}

void Carol::Model::SetAnimationClip(const std::wstring& clipName)
{
	if (!mSkinned || mAnimationClips.count(clipName) == 0)
	{
		return;
	}

	mClipName = clipName;
	mTimePos = 0.0f;

	for (auto& [name, mesh] : mMeshes)
	{
		mesh->SetAnimationClip(clipName);
	}
}

void Carol::Model::Update(Timer* timer)
{
	mTimePos += timer->DeltaTime();

	if (mTimePos > mAnimationClips[mClipName]->GetClipEndTime())
	{
		mTimePos = 0;
	};

	auto& clip = mAnimationClips[mClipName];
	uint32_t boneCount = mBoneHierarchy.size();

	vector<XMFLOAT4X4> toParentTransforms(boneCount);
	vector<XMFLOAT4X4> toRootTransforms(boneCount);
	vector<XMFLOAT4X4> finalTransforms(boneCount);
	clip->Interpolate(mTimePos, toParentTransforms);

	for (int i = 0; i < boneCount; ++i)
	{
		XMMATRIX toParent = XMLoadFloat4x4(&toParentTransforms[i]);
		XMMATRIX parentToRoot = mBoneHierarchy[i] != -1 ? XMLoadFloat4x4(&toRootTransforms[mBoneHierarchy[i]]) : XMMatrixIdentity();

		XMMATRIX toRoot = toParent * parentToRoot;
		XMStoreFloat4x4(&toRootTransforms[i], toRoot);
	}

	for (int i = 0; i < boneCount; ++i)
	{
		XMMATRIX offset = XMLoadFloat4x4(&mBoneOffsets[i]);
		XMMATRIX toRoot = XMLoadFloat4x4(&toRootTransforms[i]);

		XMStoreFloat4x4(&finalTransforms[i], XMMatrixTranspose(XMMatrixMultiply(offset, toRoot)));
	}

	std::copy(std::begin(mSkinnedConstants->FinalTransforms), std::end(mSkinnedConstants->FinalTransforms), mSkinnedConstants->HistFinalTransforms);
	std::copy(std::begin(finalTransforms), std::end(finalTransforms), mSkinnedConstants->FinalTransforms);
}

void Carol::Model::GetSkinnedVertices(const wstring& clipName, const vector<Vertex>& vertices, vector<vector<Vertex>>& skinnedVertices)
{
	auto& finalTransforms = mFinalTransforms[clipName];
	skinnedVertices.resize(finalTransforms.size());
	std::ranges::for_each(skinnedVertices, [&](vector<Vertex>& v) {v.resize(vertices.size()); });

	for (int i = 0; i < finalTransforms.size(); ++i)
	{
		for (int j = 0; j < vertices.size(); ++j)
		{
			auto& vertex = vertices[j];
			auto& skinnedVertex = skinnedVertices[i][j];

			if (vertex.Weights.x == 0.f)
			{
				skinnedVertices[i][j] = vertex;
			}
			else
			{
				float weights[] =
				{
					vertex.Weights.x,
					vertex.Weights.y,
					vertex.Weights.z,
					1 - vertex.Weights.x - vertex.Weights.y - vertex.Weights.z
				};

				XMMATRIX transform[] =
				{
					XMLoadFloat4x4(&finalTransforms[i][vertex.BoneIndices.x]),
					XMLoadFloat4x4(&finalTransforms[i][vertex.BoneIndices.y]),
					XMLoadFloat4x4(&finalTransforms[i][vertex.BoneIndices.z]),
					XMLoadFloat4x4(&finalTransforms[i][vertex.BoneIndices.w])

				};

				XMVECTOR pos = { vertex.Pos.x,vertex.Pos.y,vertex.Pos.z,1.f };
				XMVECTOR normal = { vertex.Normal.x,vertex.Normal.y,vertex.Normal.z,0.f };
				XMVECTOR skinnedPos = { 0.f,0.f,0.f,0.f };
				XMVECTOR skinnedNormal = { 0.f,0.f,0.f,0.f };

				for (int k = 0; k < 4; ++k)
				{
					if (weights[k] == 0.f)
					{
						break;
					}

					skinnedPos += weights[k] * XMVector4Transform(pos, transform[k]);
					skinnedNormal += weights[k] * XMVector4Transform(normal, transform[k]);
				}

				XMStoreFloat3(&skinnedVertex.Pos, skinnedPos);
				XMStoreFloat3(&skinnedVertex.Normal, skinnedNormal);
			}
		}
	}
}

Carol::SkinnedConstants* Carol::Model::GetSkinnedConstants()
{
	return mSkinnedConstants.get();
}

void Carol::Model::SetSkinnedCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr)
{
	for (auto& [name, mesh] : mMeshes)
	{
		mesh->SetSkinnedCBAddress(addr);
	}
}

bool Carol::Model::IsSkinned()
{
	return mSkinned;
}

void Carol::Model::LoadGround(TextureManager* texManager)
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

	vector<Vertex> vertices(4);
	vector<pair<wstring, vector<vector<Vertex>>>> skinnedVertices;

	for (int i = 0; i < 4; ++i)
	{
		vertices[i] = { pos[i],normal,tangent,texC[i] };
	}
	
	vector<uint32_t> indices = { 0,1,2,0,2,3 };

	mMeshes[L"Ground"] = make_unique<Mesh>(
		vertices,
		skinnedVertices,
		indices,
		false,
		false);

	mMeshes[L"Ground"]->SetDiffuseMapIdx(texManager->LoadTexture(L"texture\\tile.dds"));
	mMeshes[L"Ground"]->SetNormalMapIdx(texManager->LoadTexture(L"texture\\tile_nmap.dds"));
}

void Carol::Model::LoadSkyBox(TextureManager* texManager)
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

	vector<Vertex> vertices(8);
	vector<pair<wstring, vector<vector<Vertex>>>> skinnedVertices;

	for (int i = 0; i < 8; ++i)
	{
		vertices[i] = { pos[i] };
	}

	vector<uint32_t> indices = { 
		0,1,2,
		0,2,3,
		4,5,1,
		4,1,0,
		7,6,5,
		7,5,4,
		3,2,6,
		3,6,7,
		1,5,6,
		1,6,2,
		4,0,3,
		4,3,7 };

	mMeshes[L"SkyBox"] = make_unique<Mesh>(
		vertices,
		skinnedVertices,
		indices,
		false,
		false);
	mMeshes[L"SkyBox"]->SetDiffuseMapIdx(texManager->LoadTexture(L"texture\\snowcube1024.dds"));
}
