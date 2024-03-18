#include <scene/mesh.h>
#include <dx12/heap.h>
#include <dx12/resource.h>
#include <global.h>

namespace
{
	using DirectX::operator-;
	using DirectX::operator*;
}

void BoundingBoxCompare(const DirectX::XMFLOAT3& pos, DirectX::XMFLOAT3& boxMin, DirectX::XMFLOAT3& boxMax)
{
	boxMin.x = std::fmin(boxMin.x, pos.x);
	boxMin.y = std::fmin(boxMin.y, pos.y);
	boxMin.z = std::fmin(boxMin.z, pos.z);

	boxMax.x = std::fmax(boxMax.x, pos.x);
	boxMax.y = std::fmax(boxMax.y, pos.y);
	boxMax.z = std::fmax(boxMax.z, pos.z);
}

void RadiusCompare(const DirectX::XMVECTOR& pos, const DirectX::XMVECTOR& center, const DirectX::XMVECTOR& normalCone, float tanConeSpread, float& radius)
{
	float height = DirectX::XMVector3Dot(pos - center, normalCone).m128_f32[0];
	float posRadius = DirectX::XMVector3Length(pos - center - normalCone * height).m128_f32[0] - height * tanConeSpread;
	
	if (posRadius > radius)
	{
		radius = posRadius;
	}
}

Carol::Mesh::Mesh(
	std::span<Vertex> vertices,
	std::span<std::pair<std::string, std::vector<std::vector<Vertex>>>> skinnedVertices,
	std::span<uint32_t> indices,
	bool isSkinned,
	bool isTransparent)
	:mVertices(vertices),
	mSkinnedVertices(skinnedVertices),
	mIndices(indices),
	mMeshConstants(std::make_unique<MeshConstants>()),
	mSkinned(isSkinned),
	mTransparent(isTransparent)
{
	LoadVertices();
	LoadMeshlets();
	LoadCullData();
	InitCullMark();
	ReleaseIntermediateBuffer();
}

void Carol::Mesh::ReleaseIntermediateBuffer()
{
	mVertexBuffer->ReleaseIntermediateBuffer();
	mMeshletBuffer->ReleaseIntermediateBuffer();

	for (auto& [name, buffer] : mCullDataBuffer)
	{
		buffer->ReleaseIntermediateBuffer();
	}
}

uint32_t Carol::Mesh::GetMeshletSize()const
{
	return mMeshConstants->MeshletCount;
}

void Carol::Mesh::SetDiffuseTextureIdx(uint32_t idx)
{
	mMeshConstants->DiffuseTextureIdx = idx;
}

void Carol::Mesh::SetNormalTextureIdx(uint32_t idx)
{
	mMeshConstants->NormalTextureIdx = idx;
}

void Carol::Mesh::SetEmissiveTextureIdx(uint32_t idx)
{
	mMeshConstants->EmissiveTextureIdx = idx;
}

void Carol::Mesh::SetMetallicRoughnessTextureIdx(uint32_t idx)
{
	mMeshConstants->MetallicRoughnessTextureIdx = idx;
}

void Carol::Mesh::Update(DirectX::XMMATRIX& world)
{
	mMeshConstants->HistWorld = mMeshConstants->World;
	DirectX::XMStoreFloat4x4(&mMeshConstants->World, DirectX::XMMatrixTranspose(world));
}

void Carol::Mesh::SetAnimationClip(std::string_view clipName)
{
	std::string name(clipName);
	mMeshConstants->CullDataBufferIdx = mCullDataBuffer[name]->GetGpuSrvIdx();
	mMeshConstants->Center = mBoundingBoxes[name].Center;
	mMeshConstants->Extents = mBoundingBoxes[name].Extents;
}

void Carol::Mesh::SetMeshCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr)
{
	mMeshCBAddr = addr;
}

void Carol::Mesh::SetSkinnedCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr)
{
	mSkinnedCBAddr = addr;
}

const Carol::MeshConstants* Carol::Mesh::GetMeshConstants()const
{
	return mMeshConstants.get();
}

D3D12_GPU_VIRTUAL_ADDRESS Carol::Mesh::GetMeshCBAddress()const
{
	return mMeshCBAddr;
}

D3D12_GPU_VIRTUAL_ADDRESS Carol::Mesh::GetSkinnedCBAddress()const
{
	return mSkinnedCBAddr;
}

bool Carol::Mesh::IsSkinned()const
{
	return mSkinned;
}

bool Carol::Mesh::IsTransparent()const
{
	return mTransparent;
}

void Carol::Mesh::LoadVertices()
{
	mVertexBuffer = std::make_unique<StructuredBuffer>(
		mVertices.size(),
		sizeof(Vertex),
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	mVertexBuffer->CopySubresources(gHeapManager->GetUploadBuffersHeap(), mVertices.data(), mVertices.size() * sizeof(Vertex));
	mMeshConstants->VertexBufferIdx = mVertexBuffer->GetGpuSrvIdx();
}

void Carol::Mesh::LoadMeshlets()
{
	Meshlet meshlet = {};
	std::vector<uint8_t> vertices(mVertices.size(), 0xff);

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

	mMeshletBuffer = std::make_unique<StructuredBuffer>(
		mMeshlets.size(),
		sizeof(Meshlet),
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	mMeshletBuffer->CopySubresources(gHeapManager->GetUploadBuffersHeap(), mMeshlets.data(), mMeshlets.size() * sizeof(Meshlet));
	mMeshConstants->MeshletBufferIdx = mMeshletBuffer->GetGpuSrvIdx();
	mMeshConstants->MeshletCount = mMeshlets.size();
}

void Carol::Mesh::LoadCullData()
{
	static std::string staticName = "mesh";
	std::vector<std::vector<Vertex>> vertices;
	std::pair<std::string, std::vector<std::vector<Vertex>>> staticpair;

	if (!mSkinned)
	{
		vertices.emplace_back(mVertices.begin(), mVertices.end());
		staticpair = std::make_pair(staticName, std::move(vertices));
		mSkinnedVertices = std::span(&staticpair, 1);
	}

	for (auto& [name, vertices] : mSkinnedVertices)
	{
		mCullData[name].resize(mMeshlets.size());
		LoadMeshletBoundingBox(name, vertices);
		LoadMeshletNormalCone(name, vertices);

		mCullDataBuffer[name] = std::make_unique<StructuredBuffer>(
			mCullData[name].size(),
			sizeof(CullData),
			gHeapManager->GetDefaultBuffersHeap(),
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		mCullDataBuffer[name]->CopySubresources(gHeapManager->GetUploadBuffersHeap(), mCullData[name].data(), mCullData[name].size() * sizeof(CullData));
	}

	if (!mSkinned)
	{
		mMeshConstants->CullDataBufferIdx = mCullDataBuffer[staticName]->GetGpuSrvIdx();
		mMeshConstants->Center = mBoundingBoxes[staticName].Center;
		mMeshConstants->Extents = mBoundingBoxes[staticName].Extents;
	}
}

void Carol::Mesh::InitCullMark()
{
	uint32_t byteSize = ceilf(mMeshlets.size() / 8.f);

	mMeshletFrustumCulledMarkBuffer = std::make_unique<RawBuffer>(
		byteSize,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mMeshConstants->MeshletFrustumCulledMarkBufferIdx = mMeshletFrustumCulledMarkBuffer->GetGpuUavIdx();

	mMeshletNormalConeCulledMarkBuffer = std::make_unique<RawBuffer>(
		byteSize,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mMeshConstants->MeshletNormalConeCulledMarkBufferIdx = mMeshletNormalConeCulledMarkBuffer->GetGpuUavIdx();

	mMeshletOcclusionCulledMarkBuffer = std::make_unique<RawBuffer>(
		byteSize,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	
	mMeshConstants->MeshletOcclusionCulledMarkBufferIdx = mMeshletOcclusionCulledMarkBuffer->GetGpuUavIdx();

	mMeshletCulledMarkBuffer = std::make_unique<RawBuffer>(
		byteSize,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	
	mMeshConstants->MeshletCulledMarkBufferIdx = mMeshletCulledMarkBuffer->GetGpuUavIdx();
}

void Carol::Mesh::LoadMeshletBoundingBox(std::string_view clipName, std::span<std::vector<Vertex>> vertices)
{
	std::string name(clipName);
	DirectX::XMFLOAT3 meshBoxMin = { D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX };
	DirectX::XMFLOAT3 meshBoxMax = { -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX };

	for (int i = 0; i < mMeshlets.size(); ++i)
	{
		auto& meshlet = mMeshlets[i];
		DirectX::XMFLOAT3 meshletBoxMin = { D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX };
		DirectX::XMFLOAT3 meshletBoxMax = { -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX };
		
		for (auto& criticalFrameVertices : vertices)
		{
			for (int j = 0; j < meshlet.VertexCount; ++j)
			{
				auto& pos = criticalFrameVertices[meshlet.Vertices[j]].Pos;
				BoundingBoxCompare(pos, meshBoxMin, meshBoxMax);
				BoundingBoxCompare(pos, meshletBoxMin, meshletBoxMax);
			}
		}

		DirectX::BoundingBox box;
		DirectX::BoundingBox::CreateFromPoints(box, DirectX::XMLoadFloat3(&meshletBoxMin), DirectX::XMLoadFloat3(&meshletBoxMax));

		mCullData[name][i].Center = box.Center;
		mCullData[name][i].Extent = box.Extents;
	}
	
	DirectX::BoundingBox::CreateFromPoints(mBoundingBoxes[name], DirectX::XMLoadFloat3(&meshBoxMin), DirectX::XMLoadFloat3(&meshBoxMax));
}

void Carol::Mesh::LoadMeshletNormalCone(std::string_view clipName, std::span<std::vector<Vertex>> vertices)
{
	std::string name(clipName);

	for (int i = 0; i < mMeshlets.size(); ++i)
	{
		auto& meshlet = mMeshlets[i];
		DirectX::XMVECTOR normalCone = LoadConeCenter(meshlet, vertices);
		float cosConeSpread = LoadConeSpread(meshlet, normalCone, vertices);

		if (cosConeSpread <= 0.f)
		{
			mCullData[name][i].NormalCone = { 0.f,0.f,0.f,1.f };
		}
		else
		{
			float sinConeSpread = std::sqrt(1.f - cosConeSpread * cosConeSpread);
			float tanConeSpread = sinConeSpread / cosConeSpread;

			mCullData[name][i].NormalCone = {
				(normalCone.m128_f32[2] + 1.f) * 0.5f,
				(normalCone.m128_f32[1] + 1.f) * 0.5f,
				(normalCone.m128_f32[0] + 1.f) * 0.5f,
				sinConeSpread
			};
			
			float bottomDist = LoadConeBottomDist(meshlet, normalCone, vertices);
			DirectX::XMVECTOR center = DirectX::XMLoadFloat3(&mCullData[name][i].Center);

			float centerToBottomDist = DirectX::XMVector3Dot(center, normalCone).m128_f32[0] - bottomDist;
			DirectX::XMVECTOR bottomCenter = center - centerToBottomDist * normalCone;
			float radius = LoadBottomRadius(meshlet, bottomCenter, normalCone, tanConeSpread, vertices);
			
			mCullData[name][i].ApexOffset = centerToBottomDist + radius / tanConeSpread;
		}
	}
}

DirectX::XMVECTOR Carol::Mesh::LoadConeCenter(const Meshlet& meshlet, std::span<std::vector<Vertex>> vertices)
{
	DirectX::XMFLOAT3 normalBoxMin = { D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX };
	DirectX::XMFLOAT3 normalBoxMax = { -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX };

	if (mSkinned)
	{
		for (auto& criticalFrameVertices : vertices)
		{
			for (int i = 0; i < meshlet.VertexCount; ++i)
			{
				auto& normal = criticalFrameVertices[meshlet.Vertices[i]].Normal;
				BoundingBoxCompare(normal, normalBoxMin, normalBoxMax);
			}
		}
	}
	
	DirectX::BoundingBox box;
	DirectX::BoundingBox::CreateFromPoints(box, DirectX::XMLoadFloat3(&normalBoxMin), DirectX::XMLoadFloat3(&normalBoxMax));
	return DirectX::XMLoadFloat3(&box.Center);
}

float Carol::Mesh::LoadConeSpread(const Meshlet& meshlet, const DirectX::XMVECTOR& normalCone, std::span<std::vector<Vertex>> vertices)
{
	float cosConeSpread = 1.f;

	for (auto& criticalFrameVertices : vertices)
	{
		for (int i = 0; i < meshlet.VertexCount; ++i)
		{
			auto normal = DirectX::XMLoadFloat3(&criticalFrameVertices[meshlet.Vertices[i]].Normal);
			cosConeSpread = std::fmin(cosConeSpread, DirectX::XMVector3Dot(normalCone, DirectX::XMVector3Normalize(normal)).m128_f32[0]);
		}
	}

	return cosConeSpread;
}

float Carol::Mesh::LoadConeBottomDist(const Meshlet& meshlet, const DirectX::XMVECTOR& normalCone, std::span<std::vector<Vertex>> vertices)
{
	float bd = D3D12_FLOAT32_MAX;

	for (auto& criticalFrameVertices : vertices)
	{
		for (int i = 0; i < meshlet.VertexCount; ++i)
		{
			auto pos = DirectX::XMLoadFloat3(&criticalFrameVertices[meshlet.Vertices[i]].Pos);
			float dot = DirectX::XMVector3Dot(normalCone, pos).m128_f32[0];

			if (dot < bd)
			{
				bd = dot;
			}
		}
	}

	return bd;
}

float Carol::Mesh::LoadBottomRadius(const Meshlet& meshlet, const DirectX::XMVECTOR& center, const DirectX::XMVECTOR& normalCone, float tanConeSpread, std::span<std::vector<Vertex>> vertices)
{
	float radius = 0.f;

	for (auto& criticalFrameVertices : vertices)
	{
		for (int i = 0; i < meshlet.VertexCount; ++i)
		{
			auto pos = DirectX::XMLoadFloat3(&criticalFrameVertices[meshlet.Vertices[i]].Pos);
			RadiusCompare(pos, center, normalCone, tanConeSpread, radius);
		}
	}

	return radius;
}
