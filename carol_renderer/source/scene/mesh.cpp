#include <scene/mesh.h>
#include <global.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/descriptor.h>

namespace Carol
{
	using std::vector;
	using std::unique_ptr;
	using std::make_unique;
	using std::span;
	using std::pair;
	using std::wstring;
	using std::wstring_view;
	using std::unordered_map;
	using namespace DirectX;
	void BoundingBoxCompare(const XMFLOAT3& pos, XMFLOAT3& boxMin, XMFLOAT3& boxMax);
	void RadiusCompare(const XMVECTOR& pos, const XMVECTOR& center, const XMVECTOR& normalCone, float tanConeSpread, float& radius);
}

void Carol::BoundingBoxCompare(const XMFLOAT3& pos, XMFLOAT3& boxMin, XMFLOAT3& boxMax)
{
	boxMin.x = std::min(boxMin.x, pos.x);
	boxMin.y = std::min(boxMin.y, pos.y);
	boxMin.z = std::min(boxMin.z, pos.z);

	boxMax.x = std::max(boxMax.x, pos.x);
	boxMax.y = std::max(boxMax.y, pos.y);
	boxMax.z = std::max(boxMax.z, pos.z);
}

void Carol::RadiusCompare(const XMVECTOR& pos, const XMVECTOR& center, const XMVECTOR& normalCone, float tanConeSpread, float& radius)
{
	float height = XMVector3Dot(pos - center, normalCone).m128_f32[0];
	float posRadius = XMVector3Length(pos - center - normalCone * height).m128_f32[0] - height * tanConeSpread;
	
	if (posRadius > radius)
	{
		radius = posRadius;
	}
}

Carol::Mesh::Mesh(
	span<Vertex> vertices,
	span<pair<wstring, vector<vector<Vertex>>>> skinnedVertices,
	span<uint32_t> indices,
	bool isSkinned,
	bool isTransparent,
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	Heap* defaultBuffersHeap,
	Heap* uploadBuffersHeap,
	DescriptorManager* descriptorManager)
	:mVertices(vertices),
	mSkinnedVertices(skinnedVertices),
	mIndices(indices),
	mMeshConstants(make_unique<MeshConstants>()),
	mSkinned(isSkinned),
	mTransparent(isTransparent)
{
	LoadVertices(device, cmdList, defaultBuffersHeap, uploadBuffersHeap, descriptorManager);
	LoadMeshlets(device, cmdList, defaultBuffersHeap, uploadBuffersHeap, descriptorManager);
	LoadCullData(device, cmdList, defaultBuffersHeap, uploadBuffersHeap, descriptorManager);
	InitCullMark(device, cmdList, defaultBuffersHeap, uploadBuffersHeap, descriptorManager);
}

void Carol::Mesh::ReleaseIntermediateBuffer()
{
	mVertexBuffer->ReleaseIntermediateBuffer();
	mMeshletBuffer->ReleaseIntermediateBuffer();

	for (auto& [name, buffer] : mCullDataBuffer)
	{
		buffer->ReleaseIntermediateBuffer();
	}

	mMeshlets.clear();
	mMeshlets.shrink_to_fit();

	for (auto& [name, data] : mCullData)
	{
		data.clear();
		data.shrink_to_fit();
	}
}

uint32_t Carol::Mesh::GetMeshletSize()const
{
	return mMeshConstants->MeshletCount;
}

void Carol::Mesh::SetDiffuseMapIdx(uint32_t idx)
{
	mMeshConstants->DiffuseMapIdx = idx;
}

void Carol::Mesh::SetNormalMapIdx(uint32_t idx)
{
	mMeshConstants->NormalMapIdx = idx;
}

void Carol::Mesh::SetRoughnessMapIdx(uint32_t idx)
{
	mMeshConstants->RoughnessMapIdx = idx;
}

void Carol::Mesh::SetMetallicMapIdx(uint32_t idx)
{
	mMeshConstants->MetallicMapIdx = idx;
}

void Carol::Mesh::Update(XMMATRIX& world)
{
	mMeshConstants->HistWorld = mMeshConstants->World;
	XMStoreFloat4x4(&mMeshConstants->World, XMMatrixTranspose(world));
}

void Carol::Mesh::ClearCullMark(ID3D12GraphicsCommandList* cmdList)
{
	static const uint32_t cullMarkClearValue = 0;
	cmdList->ClearUnorderedAccessViewUint(mMeshletFrustumCulledMarkBuffer->GetGpuUav(), mMeshletFrustumCulledMarkBuffer->GetCpuUav(), mMeshletFrustumCulledMarkBuffer->Get(), &cullMarkClearValue, 0, nullptr);
	cmdList->ClearUnorderedAccessViewUint(mMeshletOcclusionPassedMarkBuffer->GetGpuUav(), mMeshletOcclusionPassedMarkBuffer->GetCpuUav(), mMeshletOcclusionPassedMarkBuffer->Get(), &cullMarkClearValue, 0, nullptr);
}

void Carol::Mesh::SetAnimationClip(std::wstring_view clipName)
{
	wstring name(clipName);
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

void Carol::Mesh::LoadVertices(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	Heap* defaultBuffersHeap,
	Heap* uploadBuffersHeap,
	DescriptorManager* descriptorManager)
{
	mVertexBuffer = make_unique<StructuredBuffer>(
		mVertices.size(),
		sizeof(Vertex),
		device,
		defaultBuffersHeap,
		descriptorManager,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	mVertexBuffer->CopySubresources(cmdList, uploadBuffersHeap, mVertices.data(), mVertices.size() * sizeof(Vertex));
	mMeshConstants->VertexBufferIdx = mVertexBuffer->GetGpuSrvIdx();
}

void Carol::Mesh::LoadMeshlets(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	Heap* defaultBuffersHeap,
	Heap* uploadBuffersHeap,
	DescriptorManager* descriptorManager)
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
		device,
		defaultBuffersHeap,
		descriptorManager,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	mMeshletBuffer->CopySubresources(cmdList, uploadBuffersHeap, mMeshlets.data(), mMeshlets.size() * sizeof(Meshlet));
	mMeshConstants->MeshletBufferIdx = mMeshletBuffer->GetGpuSrvIdx();
	mMeshConstants->MeshletCount = mMeshlets.size();
}

void Carol::Mesh::LoadCullData(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	Heap* defaultBuffersHeap,
	Heap* uploadBuffersHeap,
	DescriptorManager* descriptorManager)
{
	static wstring staticName = L"mesh";
	vector<vector<Vertex>> vertices;
	pair<wstring, vector<vector<Vertex>>> staticPair;

	if (!mSkinned)
	{
		vertices.emplace_back(mVertices.begin(), mVertices.end());
		staticPair = make_pair(staticName, std::move(vertices));
		mSkinnedVertices = span(&staticPair, 1);
	}

	for (auto& [name, vertices] : mSkinnedVertices)
	{
		mCullData[name].resize(mMeshlets.size());
		LoadMeshletBoundingBox(name, vertices);
		LoadMeshletNormalCone(name, vertices);

		mCullDataBuffer[name] = make_unique<StructuredBuffer>(
			mCullData[name].size(),
			sizeof(CullData),
			device,
			defaultBuffersHeap,
			descriptorManager,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		mCullDataBuffer[name]->CopySubresources(cmdList, uploadBuffersHeap, mCullData[name].data(), mCullData[name].size() * sizeof(CullData));	
	}

	if (!mSkinned)
	{
		mMeshConstants->CullDataBufferIdx = mCullDataBuffer[staticName]->GetGpuSrvIdx();
		mMeshConstants->Center = mBoundingBoxes[staticName].Center;
		mMeshConstants->Extents = mBoundingBoxes[staticName].Extents;
	}
}

void Carol::Mesh::InitCullMark(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	Heap* defaultBuffersHeap,
	Heap* uploadBuffersHeap,
	DescriptorManager* descriptorManager)
{
	uint32_t byteSize = ceilf(mMeshlets.size() / 8.f);

	mMeshletFrustumCulledMarkBuffer = make_unique<RawBuffer>(
		byteSize,
		device,
		defaultBuffersHeap,
		descriptorManager,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mMeshConstants->MeshletFrustumCulledMarkBufferIdx = mMeshletFrustumCulledMarkBuffer->GetGpuUavIdx();

	mMeshletOcclusionPassedMarkBuffer = make_unique<RawBuffer>(
		byteSize,
		device,
		defaultBuffersHeap,
		descriptorManager,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	
	mMeshConstants->MeshletOcclusionCulledMarkBufferIdx = mMeshletOcclusionPassedMarkBuffer->GetGpuUavIdx();
}

void Carol::Mesh::LoadMeshletBoundingBox(wstring_view clipName, span<vector<Vertex>> vertices)
{
	wstring name(clipName);
	XMFLOAT3 meshBoxMin = { D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX };
	XMFLOAT3 meshBoxMax = { -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX };

	for (int i = 0; i < mMeshlets.size(); ++i)
	{
		auto& meshlet = mMeshlets[i];
		XMFLOAT3 meshletBoxMin = { D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX };
		XMFLOAT3 meshletBoxMax = { -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX };
		
		for (auto& criticalFrameVertices : vertices)
		{
			for (int j = 0; j < meshlet.VertexCount; ++j)
			{
				auto& pos = criticalFrameVertices[meshlet.Vertices[j]].Pos;
				BoundingBoxCompare(pos, meshBoxMin, meshBoxMax);
				BoundingBoxCompare(pos, meshletBoxMin, meshletBoxMax);
			}
		}

		BoundingBox box;
		BoundingBox::CreateFromPoints(box, XMLoadFloat3(&meshletBoxMin), XMLoadFloat3(&meshletBoxMax));

		mCullData[name][i].Center = box.Center;
		mCullData[name][i].Extent = box.Extents;
	}
	
	BoundingBox::CreateFromPoints(mBoundingBoxes[name], XMLoadFloat3(&meshBoxMin), XMLoadFloat3(&meshBoxMax));
}

void Carol::Mesh::LoadMeshletNormalCone(wstring_view clipName, span<vector<Vertex>> vertices)
{
	wstring name(clipName);

	for (int i = 0; i < mMeshlets.size(); ++i)
	{
		auto& meshlet = mMeshlets[i];
		XMVECTOR normalCone = LoadConeCenter(meshlet, vertices);
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
			XMVECTOR center = XMLoadFloat3(&mCullData[name][i].Center);

			float centerToBottomDist = XMVector3Dot(center, normalCone).m128_f32[0] - bottomDist;
			XMVECTOR bottomCenter = center - centerToBottomDist * normalCone;
			float radius = LoadBottomRadius(meshlet, bottomCenter, normalCone, tanConeSpread, vertices);
			
			mCullData[name][i].ApexOffset = centerToBottomDist + radius / tanConeSpread;
		}
	}
}

Carol::XMVECTOR Carol::Mesh::LoadConeCenter(const Meshlet& meshlet, span<vector<Vertex>> vertices)
{
	XMFLOAT3 normalBoxMin = { D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX };
	XMFLOAT3 normalBoxMax = { -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX };

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
	
	BoundingBox box;
	BoundingBox::CreateFromPoints(box, XMLoadFloat3(&normalBoxMin), XMLoadFloat3(&normalBoxMax));
	return XMLoadFloat3(&box.Center);
}

float Carol::Mesh::LoadConeSpread(const Meshlet& meshlet, const XMVECTOR& normalCone, span<vector<Vertex>> vertices)
{
	float cosConeSpread = 1.f;

	for (auto& criticalFrameVertices : vertices)
	{
		for (int i = 0; i < meshlet.VertexCount; ++i)
		{
			auto normal = XMLoadFloat3(&criticalFrameVertices[meshlet.Vertices[i]].Normal);
			cosConeSpread = std::min(cosConeSpread, XMVector3Dot(normalCone, XMVector3Normalize(normal)).m128_f32[0]);
		}
	}

	return cosConeSpread;
}

float Carol::Mesh::LoadConeBottomDist(const Meshlet& meshlet, const Carol::XMVECTOR& normalCone, span<vector<Vertex>> vertices)
{
	float bd = D3D12_FLOAT32_MAX;

	for (auto& criticalFrameVertices : vertices)
	{
		for (int i = 0; i < meshlet.VertexCount; ++i)
		{
			auto pos = XMLoadFloat3(&criticalFrameVertices[meshlet.Vertices[i]].Pos);
			float dot = XMVector3Dot(normalCone, pos).m128_f32[0];

			if (dot < bd)
			{
				bd = dot;
			}
		}
	}

	return bd;
}

float Carol::Mesh::LoadBottomRadius(const Meshlet& meshlet, const DirectX::XMVECTOR& center, const DirectX::XMVECTOR& normalCone, float tanConeSpread, span<vector<Vertex>> vertices)
{
	float radius = 0.f;

	for (auto& criticalFrameVertices : vertices)
	{
		for (int i = 0; i < meshlet.VertexCount; ++i)
		{
			auto pos = XMLoadFloat3(&criticalFrameVertices[meshlet.Vertices[i]].Pos);
			RadiusCompare(pos, center, normalCone, tanConeSpread, radius);
		}
	}

	return radius;
}
