#include <scene/model.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/indirect_command.h>
#include <scene/mesh.h>
#include <scene/assimp.h>
#include <scene/texture.h>
#include <scene/skinned_animation.h>
#include <scene/timer.h>
#include <global.h>
#include <cmath>
#include <algorithm>
#include <ranges>

namespace
{
	using DirectX::operator*;
	using DirectX::operator+=;
}

Carol::ModelNode::ModelNode()
{
	DirectX::XMStoreFloat4x4(&Transformation, DirectX::XMMatrixIdentity());
}


Carol::Model::Model()
	:mSkinnedConstants(std::make_unique<SkinnedConstants>())
{
	
}

Carol::Model::~Model()
{
	for (auto& path : mTexturePath)
	{
		gTextureManager->UnloadTexture(path);
	}
}

const Carol::Mesh* Carol::Model::GetMesh(std::string_view meshName)const
{
	return mMeshes.at(meshName.data()).get();
}

const std::unordered_map<std::string, std::unique_ptr<Carol::Mesh>>& Carol::Model::GetMeshes()const
{
	return mMeshes;
}

std::vector<std::string_view> Carol::Model::GetAnimationClips()const
{
	std::vector<std::string_view> animations;

	for (auto& [name, animation] : mAnimationClips)
	{
		animations.push_back(std::string_view(name.c_str(),name.size()));
	}

	return animations;
}

void Carol::Model::SetAnimationClip(std::string_view clipName)
{
	if (!mSkinned || mAnimationClips.count(clipName.data()) == 0)
	{
		return;
	}

	mClipName = clipName;
	mTimePos = 0.0f;

	for (auto& [name, mesh] : mMeshes)
	{
		if (mesh->IsSkinned())
		{
			mesh->SetAnimationClip(mClipName);
		}
	}
}

void Carol::Model::Update(Timer* timer)
{
	if (mSkinned)
	{
		mTimePos += timer->DeltaTime();

		if (mTimePos > mAnimationClips[mClipName]->GetClipEndTime())
		{
			mTimePos = mAnimationClips[mClipName]->GetClipStartTime();
		};

		std::vector<DirectX::XMFLOAT4X4> finalTransforms;
		GetFinalTransforms(mClipName, mTimePos, finalTransforms);

		for (int i = 0; i < mBoneHierarchy.size(); ++i)
		{
			DirectX::XMStoreFloat4x4(&finalTransforms[i], DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&finalTransforms[i])));
		}

		std::copy(std::begin(mSkinnedConstants->FinalTransforms), std::end(mSkinnedConstants->FinalTransforms), mSkinnedConstants->HistFinalTransforms);
		std::copy(std::begin(finalTransforms), std::end(finalTransforms), mSkinnedConstants->FinalTransforms);
	}
}

void Carol::Model::GetFinalTransforms(std::string_view clipName, float t, std::vector<DirectX::XMFLOAT4X4>& finalTransforms)
{
	auto& clip = mAnimationClips[std::string(clipName)];
	uint32_t boneCount = mBoneHierarchy.size();

	std::vector<DirectX::XMFLOAT4X4> toParentTransforms(boneCount);
	std::vector<DirectX::XMFLOAT4X4> toRootTransforms(boneCount);
	finalTransforms.resize(boneCount);
	clip->Interpolate(t, toParentTransforms);

	for (int i = 0; i < boneCount; ++i)
	{
		DirectX::XMMATRIX toParent = DirectX::XMLoadFloat4x4(&toParentTransforms[i]);
		DirectX::XMMATRIX parentToRoot = mBoneHierarchy[i] != -1 ? DirectX::XMLoadFloat4x4(&toRootTransforms[mBoneHierarchy[i]]) : DirectX::XMMatrixIdentity();

		DirectX::XMMATRIX toRoot = toParent * parentToRoot;
		DirectX::XMStoreFloat4x4(&toRootTransforms[i], toRoot);
	}

	for (int i = 0; i < mBoneHierarchy.size(); ++i)
	{
		DirectX::XMMATRIX offset = DirectX::XMLoadFloat4x4(&mBoneOffsets[i]);
		DirectX::XMMATRIX toRoot = DirectX::XMLoadFloat4x4(&toRootTransforms[i]);

		DirectX::XMStoreFloat4x4(&finalTransforms[i], DirectX::XMMatrixMultiply(offset, toRoot));
	}
}

void Carol::Model::GetSkinnedVertices(std::string_view clipName, std::span<Vertex> vertices, std::vector<std::vector<Vertex>>& skinnedVertices)const
{
	auto& frameTransforms = mFrameTransforms.at(clipName.data());
	skinnedVertices.resize(frameTransforms.size());
	std::ranges::for_each(skinnedVertices, [&](std::vector<Vertex>& v) {v.resize(vertices.size()); });

	for (int i = 0; i < frameTransforms.size(); ++i)
	{
		auto& finalTransforms = frameTransforms[i];

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

				DirectX::XMMATRIX transform[] =
				{
					DirectX::XMLoadFloat4x4(&finalTransforms[vertex.BoneIndices.x]),
					DirectX::XMLoadFloat4x4(&finalTransforms[vertex.BoneIndices.y]),
					DirectX::XMLoadFloat4x4(&finalTransforms[vertex.BoneIndices.z]),
					DirectX::XMLoadFloat4x4(&finalTransforms[vertex.BoneIndices.w])

				};

				DirectX::XMVECTOR pos = { vertex.Pos.x,vertex.Pos.y,vertex.Pos.z,1.f };
				DirectX::XMVECTOR normal = { vertex.Normal.x,vertex.Normal.y,vertex.Normal.z,0.f };
				DirectX::XMVECTOR skinnedPos = { 0.f,0.f,0.f,0.f };
				DirectX::XMVECTOR skinnedNormal = { 0.f,0.f,0.f,0.f };

				for (int k = 0; k < 4; ++k)
				{
					if (weights[k] == 0.f)
					{
						break;
					}

					skinnedPos += weights[k] * DirectX::XMVector4Transform(pos, transform[k]);
					skinnedNormal += weights[k] * DirectX::XMVector4Transform(normal, transform[k]);
				}

				DirectX::XMStoreFloat3(&skinnedVertex.Pos, skinnedPos);
				DirectX::XMStoreFloat3(&skinnedVertex.Normal, skinnedNormal);
			}
		}
	}
}

const Carol::SkinnedConstants* Carol::Model::GetSkinnedConstants()const
{
	return mSkinnedConstants.get();
}

void Carol::Model::SetMeshCBAddress(std::string_view meshName, D3D12_GPU_VIRTUAL_ADDRESS addr)
{
	mMeshes[meshName.data()]->SetMeshCBAddress(addr);
}

void Carol::Model::SetSkinnedCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr)
{
	for (auto& [name, mesh] : mMeshes)
	{
		mesh->SetSkinnedCBAddress(addr);
	}
}

bool Carol::Model::IsSkinned()const
{
	return mSkinned;
}

Carol::ModelManager::ModelManager(
	std::string_view name)
	:mRootNode(std::make_unique<ModelNode>()),
	mMeshes(MESH_TYPE_COUNT),
	mIndirectCommandBuffer(MESH_TYPE_COUNT),
	mMeshBuffer(MESH_TYPE_COUNT),
	mInstanceFrustumCulledMarkBuffer(MESH_TYPE_COUNT),
	mInstanceOcclusionCulledMarkBuffer(MESH_TYPE_COUNT),
	mInstanceCulledMarkBuffer(MESH_TYPE_COUNT)
{
	mRootNode->Name = name;
	InitBuffers();
}

void Carol::ModelManager::InitBuffers()
{
	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		mInstanceFrustumCulledMarkBuffer[i] = std::make_unique<RawBuffer>(
			2 << 16,
			gHeapManager->GetDefaultBuffersHeap(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		mInstanceOcclusionCulledMarkBuffer[i] = std::make_unique<RawBuffer>(
			2 << 16,
			gHeapManager->GetDefaultBuffersHeap(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		mInstanceCulledMarkBuffer[i] = std::make_unique<RawBuffer>(
			2 << 16,
			gHeapManager->GetDefaultBuffersHeap(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	}

	mIndirectCommandBufferAllocator = std::make_unique<FrameBufferAllocator>(
		1024,
		sizeof(IndirectCommand),
		gHeapManager->GetUploadBuffersHeap(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_NONE,
		false);

	mMeshBufferAllocator = std::make_unique<FrameBufferAllocator>(
		1024,
		sizeof(MeshConstants),
		gHeapManager->GetUploadBuffersHeap(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_NONE,
		true);

	mSkinnedBufferAllocator = std::make_unique<FrameBufferAllocator>(
		1024,
		sizeof(SkinnedConstants),
		gHeapManager->GetUploadBuffersHeap(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_NONE,
		true);
}

std::vector<std::string_view> Carol::ModelManager::GetAnimationClips(std::string_view modelName)const
{
	return mModels.at(modelName.data())->GetAnimationClips();
}

std::vector<std::string_view> Carol::ModelManager::GetModelNames()const
{
	std::vector<std::string_view> models;
	for (auto& [name, model] : mModels)
	{
		models.push_back(std::string_view(name.c_str(), name.size()));
	}

	return models;
}

bool Carol::ModelManager::IsAnyOpaqueMeshes() const
{
	return mMeshes[OPAQUE_STATIC].size() + mMeshes[OPAQUE_SKINNED].size();
}

bool Carol::ModelManager::IsAnyTransparentMeshes()const
{
	return mMeshes[TRANSPARENT_STATIC].size() + mMeshes[TRANSPARENT_SKINNED].size();
}

void Carol::ModelManager::LoadModel(
	std::string_view name,
	std::string_view path,
	std::string_view textureDir,
	bool isSkinned)
{
	mRootNode->Children.push_back(std::make_unique<ModelNode>());
	auto& node = mRootNode->Children.back();
	node->Children.push_back(std::make_unique<ModelNode>());

	node->Name = name;
	mModels[node->Name] = std::make_unique<AssimpModel>(
		node.get(),
		path,
		textureDir,
		isSkinned);

	for (auto& [name, mesh] : mModels[node->Name]->GetMeshes())
	{
		std::string meshName = node->Name + '_' + name;
		uint32_t type = uint32_t(mesh->IsSkinned()) | (uint32_t(mesh->IsTransparent()) << 1);
		mMeshes[type][meshName] = mesh.get();
	}
}

void Carol::ModelManager::UnloadModel(std::string_view modelName)
{
	for (auto itr = mRootNode->Children.begin(); itr != mRootNode->Children.end(); ++itr)
	{
		if ((*itr)->Name == modelName)
		{
			mRootNode->Children.erase(itr);
			break;
		}
	}

	std::string name(modelName);

	for (auto& [meshName, mesh] : mModels[name]->GetMeshes())
	{
		std::string modelMeshName = name + "_" + meshName;
		uint32_t type = uint32_t(mesh->IsSkinned()) | (uint32_t(mesh->IsTransparent()) << 1);
		mMeshes[type].erase(modelMeshName);
	}

	mModels.erase(name);
}

uint32_t Carol::ModelManager::GetMeshesCount(MeshType type)const
{
	return mMeshes[type].size();
}

uint32_t Carol::ModelManager::GetModelsCount()const
{
	return mModels.size();
}

void Carol::ModelManager::SetWorld(std::string_view modelName, DirectX::XMMATRIX world)
{
	for (auto& node : mRootNode->Children)
	{
		if (node->Name == modelName)
		{
			DirectX::XMStoreFloat4x4(&node->Transformation, world);
		}
	}
}

void Carol::ModelManager::SetAnimationClip(std::string_view modelName, std::string_view clipName)
{
	mModels[modelName.data()]->SetAnimationClip(clipName);
}

uint32_t Carol::ModelManager::GetMeshBufferIdx(MeshType type)const
{
	return mMeshBuffer[uint32_t(type)]->GetGpuSrvIdx();
}

uint32_t Carol::ModelManager::GetCommandBufferIdx(MeshType type)const
{
	return mIndirectCommandBuffer[uint32_t(type)]->GetGpuSrvIdx();
}

uint32_t Carol::ModelManager::GetInstanceFrustumCulledMarkBufferIdx(MeshType type)const
{
	return mInstanceFrustumCulledMarkBuffer[uint32_t(type)]->GetGpuUavIdx();
}

uint32_t Carol::ModelManager::GetInstanceOcclusionCulledMarkBufferIdx(MeshType type)const
{
	return mInstanceOcclusionCulledMarkBuffer[uint32_t(type)]->GetGpuUavIdx();
}

uint32_t Carol::ModelManager::GetInstanceCulledMarkBufferIdx(MeshType type) const
{
	return mInstanceCulledMarkBuffer[uint32_t(type)]->GetGpuUavIdx();
}

void Carol::ModelManager::Update(Timer* timer, uint64_t cpuFenceValue, uint64_t completedFenceValue)
{
	for (auto& [name, model] : mModels)
	{
		model->Update(timer);
	}

	ProcessNode(mRootNode.get(), DirectX::XMMatrixIdentity());

	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		mIndirectCommandBufferAllocator->DiscardBuffer(mIndirectCommandBuffer[i].release(), cpuFenceValue);
		mIndirectCommandBuffer[i] = mIndirectCommandBufferAllocator->RequestBuffer(completedFenceValue, gModelManager->GetMeshesCount(MeshType(i)));

		mMeshBufferAllocator->DiscardBuffer(mMeshBuffer[i].release(), cpuFenceValue);
		mMeshBuffer[i] = mMeshBufferAllocator->RequestBuffer(completedFenceValue, gModelManager->GetMeshesCount(MeshType(i)));
	}

	mSkinnedBufferAllocator->DiscardBuffer(mSkinnedBuffer.release(), cpuFenceValue);
	mSkinnedBuffer = mSkinnedBufferAllocator->RequestBuffer(completedFenceValue, GetModelsCount());

	int modelIdx = 0;
	for (auto& [name, model] : mModels)
	{
		if (model->IsSkinned())
		{
			mSkinnedBuffer->CopyElements(model->GetSkinnedConstants(), modelIdx);
			model->SetSkinnedCBAddress(mSkinnedBuffer->GetElementAddress(modelIdx));
			++modelIdx;
		}
	}

	for (int i = 0; i < MESH_TYPE_COUNT; ++i)
	{
		int meshIdx = 0;

		for (auto& [name, mesh] : mMeshes[i])
		{
			mMeshBuffer[i]->CopyElements(mesh->GetMeshConstants(), meshIdx);
			mesh->SetMeshCBAddress(mMeshBuffer[i]->GetElementAddress(meshIdx));

			IndirectCommand indirectCmd;
			
			indirectCmd.MeshCBAddr = mesh->GetMeshCBAddress();
			indirectCmd.SkinnedCBAddr = mesh->GetSkinnedCBAddress();

			indirectCmd.DispatchMeshArgs.ThreadGroupCountX = ceilf(mesh->GetMeshletSize() * 1.f / 32);
			indirectCmd.DispatchMeshArgs.ThreadGroupCountY = 1;
			indirectCmd.DispatchMeshArgs.ThreadGroupCountZ = 1;

			mIndirectCommandBuffer[i]->CopyElements(&indirectCmd, meshIdx);

			++meshIdx;
		}
	}
}

void Carol::ModelManager::ProcessNode(ModelNode* node, DirectX::XMMATRIX parentToRoot)
{
	DirectX::XMMATRIX toParent = DirectX::XMLoadFloat4x4(&node->Transformation);
	DirectX::XMMATRIX world = toParent * parentToRoot;

	for(auto& mesh : node->Meshes)
	{
		mesh->Update(world);
	}

	for (auto& child : node->Children)
	{
		ProcessNode(child.get(), world);
	}
}