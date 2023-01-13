#include <scene/model.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/descriptor.h>
#include <scene/scene.h>
#include <scene/skinned_data.h>
#include <scene/timer.h>
#include <scene/texture.h>
#include <utils/common.h>
#include <cmath>

namespace Carol
{
	using std::vector;
	using std::unique_ptr;
	using std::make_unique;
	using std::unordered_map;
	using std::wstring;
	using namespace DirectX;
}

Carol::Mesh::Mesh(
	vector<Vertex>& vertices,
	vector<uint32_t>& indices,
	bool isSkinned,
	bool isTransparent,
	Model* model, 
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	HeapManager* heapManager,
	DescriptorManager* descriptorManager)
	:mModel(model),
	mDevice(device),
	mCommandList(cmdList),
	mHeapManager(heapManager),
	mDescriptorManager(descriptorManager),
	mVertices(std::move(vertices)),
	mIndices(std::move(indices)),
	mMeshConstants(make_unique<MeshConstants>()),
	mSkinned(isSkinned),
	mTransparent(isTransparent)
{
	LoadVertices();
	LoadMeshlets();
	LoadCullData();
	InitCullMark();
}

void Carol::Mesh::ReleaseIntermediateBuffer()
{
	mVertexBuffer->ReleaseIntermediateBuffer();
	mMeshletBuffer->ReleaseIntermediateBuffer();
	mCullDataBuffer->ReleaseIntermediateBuffer();

	mVertices.clear();
	mVertices.shrink_to_fit();

	mIndices.clear();
	mIndices.shrink_to_fit();

	mMeshlets.clear();
	mMeshlets.shrink_to_fit();

	mCullData.clear();
	mCullData.shrink_to_fit();
}

Carol::Material Carol::Mesh::GetMaterial()
{
	return mMaterial;
}

uint32_t Carol::Mesh::GetMeshletSize()
{
	return mMeshConstants->MeshletCount;
}

void Carol::Mesh::SetMaterial(const Material& mat)
{
	mMaterial = mat;
}

void Carol::Mesh::SetDiffuseMapIdx(uint32_t idx)
{
	mMeshConstants->DiffuseMapIdx = idx;
}

void Carol::Mesh::SetNormalMapIdx(uint32_t idx)
{
	mMeshConstants->NormalMapIdx = idx;
}

Carol::BoundingBox Carol::Mesh::GetBoundingBox()
{
	return mBoundingBox;
}

void Carol::Mesh::SetOctreeNode(OctreeNode* node)
{
	mOctreeNode = node;
}

Carol::OctreeNode* Carol::Mesh::GetOctreeNode()
{
	return mOctreeNode;
}

void Carol::Mesh::Update(XMMATRIX& world)
{
	mMeshConstants->FresnelR0 = mMaterial.FresnelR0;
	mMeshConstants->Roughness = mMaterial.Roughness;
	mMeshConstants->HistWorld = mMeshConstants->World;
	XMStoreFloat4x4(&mMeshConstants->World, XMMatrixTranspose(world));
}

void Carol::Mesh::ClearCullMark(ID3D12GraphicsCommandList* cmdList)
{
	static const uint32_t cullMarkClearValue = 0;
	cmdList->ClearUnorderedAccessViewUint(mMeshletFrustumCulledMarkBuffer->GetGpuUav(), mMeshletFrustumCulledMarkBuffer->GetCpuUav(), mMeshletFrustumCulledMarkBuffer->Get(), &cullMarkClearValue, 0, nullptr);
	cmdList->ClearUnorderedAccessViewUint(mMeshletOcclusionPassedMarkBuffer->GetGpuUav(), mMeshletOcclusionPassedMarkBuffer->GetCpuUav(), mMeshletOcclusionPassedMarkBuffer->Get(), &cullMarkClearValue, 0, nullptr);
}

void Carol::Mesh::SetMeshCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr)
{
	mMeshCBAddr = addr;
}

Carol::MeshConstants* Carol::Mesh::GetMeshConstants()
{
	return mMeshConstants.get();
}

D3D12_GPU_VIRTUAL_ADDRESS Carol::Mesh::GetMeshCBAddress()
{
	return mMeshCBAddr;
}

D3D12_GPU_VIRTUAL_ADDRESS Carol::Mesh::GetSkinnedCBAddress()
{
	return mModel->GetSkinnedCBAddress();
}

bool Carol::Mesh::IsSkinned()
{
	return mSkinned;
}

bool Carol::Mesh::IsTransparent()
{
	return mTransparent;
}

void Carol::Mesh::LoadVertices()
{
	mVertexBuffer = make_unique<StructuredBuffer>(
		mVertices.size(),
		sizeof(Vertex),
		mHeapManager->GetDefaultBuffersHeap(),
		mDescriptorManager,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	mVertexBuffer->CopySubresources(mCommandList, mHeapManager->GetUploadBuffersHeap(), mVertices.data(), mVertices.size() * sizeof(Vertex));
	mMeshConstants->VertexBufferIdx = mVertexBuffer->GetGpuSrvIdx();
}

void Carol::Mesh::LoadMeshlets()
{
	Meshlet meshlet = {};
	vector<uint8_t> vertices(mVertices.size(), 0xff);

	for (int i = 0; i < mIndices.size(); i += 3)
	{
		uint32_t index[3] = { mIndices[i],mIndices[i + 1],mIndices[i + 2] };
		uint32_t meshletIndex[3] = { vertices[index[0]],vertices[index[1]],vertices[index[2]] };

		if (meshlet.VertexCount + (meshletIndex[0] == 0xff) + (meshletIndex[1] == 0xff) + (meshletIndex[2] == 0xff) > 64 || meshlet.PrimCount + 1 > 126)
		{
			mMeshlets.push_back(meshlet);

			meshlet = {};
			
			for (auto& index : meshletIndex)
			{
				index = 0xff;
			}

			for (auto& vertex : vertices)
			{
				vertex = 0xff;
			}
		}

		for (int j = 0; j < 3; ++j)
		{
			if (meshletIndex[j] == 0xff)
			{
				meshletIndex[j] = meshlet.VertexCount;
				meshlet.Vertices[meshlet.VertexCount] = index[j];
				vertices[index[j]] = meshlet.VertexCount++;
			}
		}	

		// Pack the indices
		meshlet.Prims[meshlet.PrimCount++] =
			(meshletIndex[0] & 0x3ff) |
			((meshletIndex[1] & 0x3ff) << 10) |
			((meshletIndex[2] & 0x3ff) << 20);
	}

	if (meshlet.PrimCount)
	{
		mMeshlets.push_back(meshlet);
	}

	mMeshletBuffer = make_unique<StructuredBuffer>(
		mMeshlets.size(),
		sizeof(Meshlet),
		mHeapManager->GetDefaultBuffersHeap(),
		mDescriptorManager,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	mMeshletBuffer->CopySubresources(mCommandList, mHeapManager->GetUploadBuffersHeap(), mMeshlets.data(), mMeshlets.size() * sizeof(Meshlet));
	mMeshConstants->MeshletBufferIdx = mMeshletBuffer->GetGpuSrvIdx();
	mMeshConstants->MeshletCount = mMeshlets.size();
}

void Carol::Mesh::LoadCullData()
{
	mCullData.resize(mMeshlets.size());
	LoadMeshletBoundingBox();
	LoadMeshletNormalCone();

	mCullDataBuffer = make_unique<StructuredBuffer>(
		mCullData.size(),
		sizeof(CullData),
		mHeapManager->GetDefaultBuffersHeap(),
		mDescriptorManager,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	mCullDataBuffer->CopySubresources(mCommandList, mHeapManager->GetUploadBuffersHeap(), mCullData.data(), mCullData.size() * sizeof(CullData));
	mMeshConstants->CullDataBufferIdx = mCullDataBuffer->GetGpuSrvIdx();
}

void Carol::Mesh::InitCullMark()
{
	uint32_t byteSize = ceilf(mMeshlets.size() / 8.f);

	mMeshletFrustumCulledMarkBuffer = make_unique<RawBuffer>(
		byteSize,
		mHeapManager->GetDefaultBuffersHeap(),
		mDescriptorManager,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mMeshConstants->MeshletFrustumCulledMarkBufferIdx = mMeshletFrustumCulledMarkBuffer->GetGpuUavIdx();

	mMeshletOcclusionPassedMarkBuffer = make_unique<RawBuffer>(
		byteSize,
		mHeapManager->GetDefaultBuffersHeap(),
		mDescriptorManager,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	
	mMeshConstants->MeshletOcclusionCulledMarkBufferIdx = mMeshletOcclusionPassedMarkBuffer->GetGpuUavIdx();
}

void Carol::Mesh::LoadMeshletBoundingBox()
{
	auto& animationTransforms = mModel->GetAnimationTransforms();
	XMFLOAT3 meshBoxMin = { D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX };
	XMFLOAT3 meshBoxMax = { -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX };

	for (int i = 0; i < mMeshlets.size(); ++i)
	{
		auto& meshlet = mMeshlets[i];
		XMFLOAT3 boxMin = { D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX };
		XMFLOAT3 boxMax = { -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX };
	
		if (mSkinned)
		{
			for (int i = 0; i < meshlet.VertexCount; ++i) {

				Vertex& vertex = mVertices[meshlet.Vertices[i]];
				XMVECTOR pos = { vertex.Pos.x,vertex.Pos.y,vertex.Pos.z,1.0f };

				float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				weights[0] = vertex.Weights.x;
				weights[1] = vertex.Weights.y;
				weights[2] = vertex.Weights.z;
				weights[3] = 1.0f - weights[0] - weights[1] - weights[2];

				if (weights[0] == 0.0f)
				{
					BoundingBoxCompare(pos, meshBoxMin, meshBoxMax);
					BoundingBoxCompare(pos, boxMin, boxMax);
					continue;
				}

				uint32_t boneIndices[4] = {
					vertex.BoneIndices.x,
					vertex.BoneIndices.y,
					vertex.BoneIndices.z,
					vertex.BoneIndices.w };

				for (auto& frames : animationTransforms)
				{
					for (auto& frame : frames)
					{
						XMVECTOR skinnedPos = { 0.0f,0.0f,0.0f,0.0f };

						for (int j = 0; j < 4 && weights[j] > 0.0f; ++j)
						{
							skinnedPos += weights[j] * XMVector4Transform(pos, XMLoadFloat4x4(&frame[boneIndices[j]]));
						}

						BoundingBoxCompare(skinnedPos, meshBoxMin, meshBoxMax);
						BoundingBoxCompare(skinnedPos, boxMin, boxMax);
					}
				}
			}
		}
		else
		{
			for (int i = 0; i < meshlet.VertexCount; ++i)
			{
				auto& vertex = mVertices[meshlet.Vertices[i]];
				XMVECTOR pos = { vertex.Pos.x,vertex.Pos.y,vertex.Pos.z };
				BoundingBoxCompare(pos, meshBoxMin, meshBoxMax);
				BoundingBoxCompare(pos, boxMin, boxMax);
			}
		}

		BoundingBox::CreateFromPoints(mBoundingBox, XMLoadFloat3(&boxMin), XMLoadFloat3(&boxMax));
		mCullData[i].Center = mBoundingBox.Center;
		mCullData[i].Extent = mBoundingBox.Extents;
	}

	BoundingBox::CreateFromPoints(mOriginalBoundingBox, XMLoadFloat3(&meshBoxMin), XMLoadFloat3(&meshBoxMax));
	mBoundingBox = mOriginalBoundingBox;
	mMeshConstants->Center = mBoundingBox.Center;
	mMeshConstants->Extents = mBoundingBox.Extents;
}

void Carol::Mesh::LoadMeshletNormalCone()
{
	auto& animationTransforms = mModel->GetAnimationTransforms();

	for (int i = 0; i < mMeshlets.size(); ++i)
	{
		auto& meshlet = mMeshlets[i];
		XMVECTOR normalCone = LoadConeCenter(meshlet);
		float cosConeSpread = LoadConeSpread(meshlet, normalCone);

		if (cosConeSpread <= 0.f)
		{
			mCullData[i].NormalCone = { 0.f,0.f,0.f,1.f };
		}
		else
		{
			float sinConeSpread = std::sqrt(1.f - cosConeSpread * cosConeSpread);
			float tanConeSpread = sinConeSpread / cosConeSpread;

			mCullData[i].NormalCone = {
				(normalCone.m128_f32[2] + 1.f) * 0.5f,
				(normalCone.m128_f32[1] + 1.f) * 0.5f,
				(normalCone.m128_f32[0] + 1.f) * 0.5f,
				sinConeSpread
			};
			
			float bottomDist = LoadConeBottomDist(meshlet, normalCone);
			XMVECTOR center = XMLoadFloat3(&mCullData[i].Center);

			float centerToBottomDist = XMVector3Dot(center, normalCone).m128_f32[0] - bottomDist;
			XMVECTOR bottomCenter = center - centerToBottomDist * normalCone;
			float radius = LoadBottomRadius(meshlet, bottomCenter, normalCone, tanConeSpread);
			
			mCullData[i].ApexOffset = centerToBottomDist + radius / tanConeSpread;
		}
	}

}

DirectX::XMVECTOR Carol::Mesh::LoadConeCenter(const Meshlet& meshlet)
{
	auto& animationTransforms = mModel->GetAnimationTransforms();
	XMFLOAT3 boxMin = { D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX };
	XMFLOAT3 boxMax = { -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX };

	if (mSkinned)
	{
		for (int i = 0; i < meshlet.VertexCount; ++i)
		{
			Vertex& vertex = mVertices[meshlet.Vertices[i]];
			XMVECTOR normal = XMVector3Normalize(XMLoadFloat3(&vertex.Normal));

			float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			weights[0] = vertex.Weights.x;
			weights[1] = vertex.Weights.y;
			weights[2] = vertex.Weights.z;
			weights[3] = 1.0f - weights[0] - weights[1] - weights[2];

			if (weights[0] == 0.0f)
			{
				BoundingBoxCompare(normal, boxMin, boxMax);
			}

			uint32_t boneIndices[4] = {
				vertex.BoneIndices.x,
				vertex.BoneIndices.y,
				vertex.BoneIndices.z,
				vertex.BoneIndices.w };

			for (auto& frames : animationTransforms)
			{
				for (auto& frame : frames)
				{
					XMVECTOR skinnedNormal = { 0.0f,0.0f,0.0f,0.0f };

					for (int j = 0; j < 4 && weights[j] > 0.0f; ++j)
					{
						skinnedNormal += weights[j] * XMVector3TransformNormal(normal, XMLoadFloat4x4(&frame[boneIndices[j]]));
					}

					BoundingBoxCompare(XMVector3Normalize(skinnedNormal), boxMin, boxMax);
				}
			}
		}
	}
	else
	{
		for (int i = 0; i < meshlet.VertexCount; ++i)
		{
			Vertex& vertex = mVertices[meshlet.Vertices[i]];
			XMVECTOR normal = XMVector3Normalize(XMLoadFloat3(&vertex.Normal));
			BoundingBoxCompare(normal, boxMin, boxMax);
		}
	}

	BoundingBox::CreateFromPoints(mBoundingBox, XMLoadFloat3(&boxMin), XMLoadFloat3(&boxMax));
	return XMLoadFloat3(&mBoundingBox.Center);
}

float Carol::Mesh::LoadConeSpread(const Meshlet& meshlet, const XMVECTOR& normalCone)
{
	auto& animationTransforms = mModel->GetAnimationTransforms();
	float cosConeSpread = 1.f;

	if (mSkinned)
	{
		for (int i = 0; i < meshlet.VertexCount; ++i)
		{
			Vertex& vertex = mVertices[meshlet.Vertices[i]];
			XMVECTOR normal = XMVector3Normalize(XMLoadFloat3(&vertex.Normal));

			float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			weights[0] = vertex.Weights.x;
			weights[1] = vertex.Weights.y;
			weights[2] = vertex.Weights.z;
			weights[3] = 1.0f - weights[0] - weights[1] - weights[2];

			if (weights[0] == 0.0f)
			{
				cosConeSpread = std::min(cosConeSpread, XMVector3Dot(normalCone, normal).m128_f32[0]);
			}

			uint32_t boneIndices[4] = {
				vertex.BoneIndices.x,
				vertex.BoneIndices.y,
				vertex.BoneIndices.z,
				vertex.BoneIndices.w };

			for (auto& frames : animationTransforms)
			{
				for (auto& frame : frames)
				{
					XMVECTOR skinnedNormal = { 0.0f,0.0f,0.0f,0.0f };

					for (int j = 0; j < 4 && weights[j] > 0.0f; ++j)
					{
						skinnedNormal += weights[j] * XMVector3TransformNormal(normal, XMLoadFloat4x4(&frame[boneIndices[j]]));
					}

					cosConeSpread = std::min(cosConeSpread, XMVector3Dot(normalCone, XMVector3Normalize(skinnedNormal)).m128_f32[0]);
				}
			}
		}
	}
	else
	{
		for (int i = 0; i < meshlet.VertexCount; ++i)
		{
			Vertex& vertex = mVertices[meshlet.Vertices[i]];
			XMVECTOR normal = XMVector3Normalize(XMLoadFloat3(&vertex.Normal));
			cosConeSpread = std::min(cosConeSpread, XMVector3Dot(normalCone, normal).m128_f32[0]);
		}
	}

	return cosConeSpread;
}

float Carol::Mesh::LoadConeBottomDist(const Meshlet& meshlet, const DirectX::XMVECTOR& normalCone)
{
	auto& animationTransforms = mModel->GetAnimationTransforms();
	float bd = D3D12_FLOAT32_MAX;

	if (mSkinned)
	{
		for (int i = 0; i < meshlet.VertexCount; ++i)
		{
			Vertex& vertex = mVertices[meshlet.Vertices[i]];
			XMVECTOR pos = { vertex.Pos.x,vertex.Pos.y,vertex.Pos.z,1.f };

			float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			weights[0] = vertex.Weights.x;
			weights[1] = vertex.Weights.y;
			weights[2] = vertex.Weights.z;
			weights[3] = 1.0f - weights[0] - weights[1] - weights[2];

			if (weights[0] == 0.0f)
			{
				float dot = XMVector3Dot(normalCone, pos).m128_f32[0];
				if (dot < bd)
				{
					bd = dot;
				}
			}

			uint32_t boneIndices[4] = {
				vertex.BoneIndices.x,
				vertex.BoneIndices.y,
				vertex.BoneIndices.z,
				vertex.BoneIndices.w };

			for (auto& frames : animationTransforms)
			{
				for (auto& frame : frames)
				{
					XMVECTOR skinnedPos = { 0.0f,0.0f,0.0f,0.0f };

					for (int j = 0; j < 4 && weights[j] > 0.0f; ++j)
					{
						skinnedPos += weights[j] * XMVector4Transform(pos, XMLoadFloat4x4(&frame[boneIndices[j]]));
					}

					float dot = XMVector3Dot(normalCone, skinnedPos).m128_f32[0];
					if (dot < bd)
					{
						bd = dot;
					}
				}
			}
		}
	}
	else
	{
		for (int i = 0; i < meshlet.VertexCount; ++i)
		{
			Vertex& vertex = mVertices[meshlet.Vertices[i]];
			XMVECTOR pos = { vertex.Pos.x,vertex.Pos.y,vertex.Pos.z,1.f };
		
			float dot = XMVector3Dot(normalCone, pos).m128_f32[0];
			if (dot < bd)
			{
				bd = dot;
			}
		}
	}

	return bd;
}

float Carol::Mesh::LoadBottomRadius(const Meshlet& meshlet, const DirectX::XMVECTOR& center, const DirectX::XMVECTOR& normalCone, const float& tanConeSpread)
{
	auto& animationTransforms = mModel->GetAnimationTransforms();
	float radius = 0.f;

	if (mSkinned)
	{
		for (int i = 0; i < meshlet.VertexCount; ++i)
		{
			Vertex& vertex = mVertices[meshlet.Vertices[i]];
			XMVECTOR pos = { vertex.Pos.x,vertex.Pos.y,vertex.Pos.z,1.f };

			float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			weights[0] = vertex.Weights.x;
			weights[1] = vertex.Weights.y;
			weights[2] = vertex.Weights.z;
			weights[3] = 1.0f - weights[0] - weights[1] - weights[2];

			if (weights[0] == 0.0f)
			{
				RadiusCompare(pos, center, normalCone, tanConeSpread, radius);
			}

			uint32_t boneIndices[4] = {
				vertex.BoneIndices.x,
				vertex.BoneIndices.y,
				vertex.BoneIndices.z,
				vertex.BoneIndices.w };

			for (auto& frames : animationTransforms)
			{
				for (auto& frame : frames)
				{
					XMVECTOR skinnedPos = { 0.0f,0.0f,0.0f,0.0f };

					for (int j = 0; j < 4 && weights[j] > 0.0f; ++j)
					{
						skinnedPos += weights[j] * XMVector4Transform(pos, XMLoadFloat4x4(&frame[boneIndices[j]]));
					}

					RadiusCompare(skinnedPos, center, normalCone, tanConeSpread, radius);
				}
			}
		}
	}
	else
	{
		for (int i = 0; i < meshlet.VertexCount; ++i)
		{
			Vertex& vertex = mVertices[meshlet.Vertices[i]];
			XMVECTOR pos = { vertex.Pos.x,vertex.Pos.y,vertex.Pos.z,1.f };
			RadiusCompare(pos, center, normalCone, tanConeSpread, radius);
		}
	}

	return radius;
}

void Carol::Mesh::BoundingBoxCompare(const DirectX::XMVECTOR& vPos, DirectX::XMFLOAT3& boxMin, DirectX::XMFLOAT3& boxMax)
{
	XMFLOAT3 pos;
	XMStoreFloat3(&pos, vPos);

	boxMin.x = std::min(boxMin.x, pos.x);
	boxMin.y = std::min(boxMin.y, pos.y);
	boxMin.z = std::min(boxMin.z, pos.z);

	boxMax.x = std::max(boxMax.x, pos.x);
	boxMax.y = std::max(boxMax.y, pos.y);
	boxMax.z = std::max(boxMax.z, pos.z);
}

void Carol::Mesh::RadiusCompare(const DirectX::XMVECTOR& pos, const DirectX::XMVECTOR& center, const DirectX::XMVECTOR& normalCone, float tanConeSpread, float& radius)
{
	float height = XMVector3Dot(pos - center, normalCone).m128_f32[0];
	float posRadius = XMVector3Length(pos - center - normalCone * height).m128_f32[0] - height * tanConeSpread;
	
	if (posRadius > radius)
	{
		radius = posRadius;
	}
}

Carol::Model::Model(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	HeapManager* heapManager,
	DescriptorManager* descriptorManager)
	:mDevice(device),
	mCommandList(cmdList),
	mHeapManager(heapManager),
	mDescriptorManager(descriptorManager),
	mSkinnedConstants(make_unique<SkinnedConstants>())
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

const Carol::vector<Carol::vector<Carol::vector<Carol::XMFLOAT4X4>>>& Carol::Model::GetAnimationTransforms()
{
	return mAnimationFrames;
}

Carol::vector<Carol::wstring> Carol::Model::GetAnimationClips()
{
	vector<wstring> animations;

	for (auto& aniMapPair : mAnimationClips)
	{
		animations.push_back(aniMapPair.first);
	}

	return animations;
}

void Carol::Model::SetAnimationClip(std::wstring clipName)
{
	mClipName = clipName;
	mTimePos = 0.0f;
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

Carol::SkinnedConstants* Carol::Model::GetSkinnedConstants()
{
	return mSkinnedConstants.get();
}

void Carol::Model::SetSkinnedCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr)
{
	mSkinnedCBAddr = addr;
}

D3D12_GPU_VIRTUAL_ADDRESS Carol::Model::GetSkinnedCBAddress()
{
	return mSkinnedCBAddr;
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
	for (int i = 0; i < 4; ++i)
	{
		vertices[i] = { pos[i],normal,tangent,texC[i] };
	}
	
	vector<uint32_t> indices = { 0,1,2,0,2,3 };

	mMeshes[L"Ground"] = make_unique<Mesh>(
		vertices,
		indices,
		false,
		false,
		this,
		mDevice,
		mCommandList,
		mHeapManager,
		mDescriptorManager);

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
	for (int i = 0; i < 8; ++i)
	{
		vertices[i] = { pos[i] };
	}

	vector<uint32_t> indices = { 0,1,2,0,2,3,4,5,1,4,1,0,7,6,5,7,5,4,3,2,6,3,6,7,1,5,6,1,6,2,4,0,3,4,3,7 };
	mMeshes[L"SkyBox"] = make_unique<Mesh>(
		vertices,
		indices,
		false,
		false,
		this,
		mDevice,
		mCommandList,
		mHeapManager,
		mDescriptorManager);
	mMeshes[L"SkyBox"]->SetDiffuseMapIdx(texManager->LoadTexture(L"texture\\snowcube1024.dds"));
}
