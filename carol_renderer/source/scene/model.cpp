#include <scene/model.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <dx12/descriptor_allocator.h>
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

Carol::Mesh::Mesh(Model* model, 
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	Heap* heap,
	Heap* uploadHeap,
	CbvSrvUavDescriptorAllocator* allocator,
	vector<Vertex>& vertices,
	vector<uint32_t>& indices,
	bool isSkinned,
	bool isTransparent)
	:mModel(model),
	mDevice(device),
	mCommandList(cmdList),
	mHeap(heap),
	mUploadHeap(uploadHeap),
	mAllocator(allocator),
	mVertices(std::move(vertices)),
	mIndices(std::move(indices)),
	mVertexSrvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mMeshletSrvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mCullDataSrvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mSkinned(isSkinned),
	mTransparent(isTransparent),
	mMeshIdx(TEX_IDX_COUNT)
{
	LoadVertices();
	LoadMeshlets();
	LoadCullData();
}

Carol::Mesh::~Mesh()
{
	
}

D3D12_GPU_VIRTUAL_ADDRESS Carol::Mesh::GetSkinnedCBGPUVirtualAddress()
{
	return mModel->GetSkinnedCBGPUVirtualAddress();
}

void Carol::Mesh::ReleaseIntermediateBuffer()
{
	mVertexBuffer->ReleaseIntermediateBuffer();
	mVertices.clear();
	mVertices.shrink_to_fit();

	mMeshletBuffer->ReleaseIntermediateBuffer();
	mMeshlets.clear();
	mMeshlets.shrink_to_fit();

	mCullDataBuffer->ReleaseIntermediateBuffer();
	mCullData.clear();
	mCullData.shrink_to_fit();
}

Carol::Material Carol::Mesh::GetMaterial()
{
	return mMaterial;
}

Carol::vector<uint32_t> Carol::Mesh::GetTexIdx()
{
	return mMeshIdx;
}

uint32_t Carol::Mesh::GetMeshletSize()
{
	return mMeshletSize;
}

void Carol::Mesh::SetMaterial(const Material& mat)
{
	mMaterial = mat;
}

void Carol::Mesh::SetTexIdx(uint32_t type, uint32_t idx)
{
	mMeshIdx[type] = idx;
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
	uint32_t vertexStride = sizeof(Vertex);
	uint32_t vbByteSize = mVertices.size() * vertexStride;

	LoadResource(
		mVertexBuffer,
		reinterpret_cast<void*>(mVertices.data()),
		vbByteSize,
		mVertices.size(),
		vertexStride,
		mVertexSrvAllocInfo.get(),
		VERTEX_IDX);
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

	uint32_t meshletStride = sizeof(Meshlet);
	uint32_t meshletByteSize = mMeshlets.size() * meshletStride;
	LoadResource(
		mMeshletBuffer,
		reinterpret_cast<void*>(mMeshlets.data()),
		meshletByteSize,
		mMeshlets.size(),
		meshletStride, mMeshletSrvAllocInfo.get(),
		MESHLET_IDX);

	mMeshletSize = mMeshlets.size();
}

void Carol::Mesh::LoadCullData()
{
	mCullData.reserve(mMeshlets.size());
	auto& animationFrames = mModel->GetAnimationFrames();

	for (auto& meshlet : mMeshlets)
	{
		XMFLOAT3 boxMin = { D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX, D3D12_FLOAT32_MAX };
		XMFLOAT3 boxMax = { -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX, -D3D12_FLOAT32_MAX };
	
		auto compare = [&](XMVECTOR skinnedPos)
		{
			XMFLOAT3 pos;
			XMStoreFloat3(&pos, skinnedPos);

			boxMin.x = std::min(boxMin.x, pos.x);
			boxMin.y = std::min(boxMin.y, pos.y);
			boxMin.z = std::min(boxMin.z, pos.z);

			boxMax.x = std::max(boxMax.x, pos.x);
			boxMax.y = std::max(boxMax.y, pos.y);
			boxMax.z = std::max(boxMax.z, pos.z);
		};

		if (mSkinned)
		{
			for (int i = 0; i < meshlet.VertexCount; ++i) {

				auto& vertex = mVertices[meshlet.Vertices[i]];
				XMVECTOR pos = { vertex.Pos.x,vertex.Pos.y,vertex.Pos.z,1.0f };

				float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				weights[0] = vertex.Weights.x;
				weights[1] = vertex.Weights.y;
				weights[2] = vertex.Weights.z;
				weights[3] = 1.0f - weights[0] - weights[1] - weights[2];

				if (weights[0] == 0.0f)
				{
					compare(pos);
					continue;
				}

				uint32_t boneIndices[4] = {
					vertex.BoneIndices.x,
					vertex.BoneIndices.y,
					vertex.BoneIndices.z,
					vertex.BoneIndices.w };

				for (auto& frames : animationFrames)
				{
					for (auto& frame : frames)
					{
						XMVECTOR skinnedPos = { 0.0f,0.0f,0.0f,0.0f };

						for (int j = 0; j < 4 && weights[j] > 0.0f; ++j)
						{
							skinnedPos += weights[j] * XMVector4Transform(pos, XMLoadFloat4x4(&frame[boneIndices[j]]));
						}

						compare(skinnedPos);
					}
				}
			}
		}
		else
		{
			for (int i = 0; i < meshlet.VertexCount; ++i)
			{
				auto& vertex = mVertices[meshlet.Vertices[i]];
				XMVECTOR pos = { vertex.Pos.x,vertex.Pos.y,vertex.Pos.z,1.0f };
				compare(pos);
			}
		}

		mCullData.emplace_back();
		mCullData.back().Center = {(boxMin.x + boxMax.x) / 2,(boxMin.y + boxMax.y) / 2,(boxMin.z + boxMin.z) / 2};
		mCullData.back().Extent = {std::abs(boxMax.x - boxMin.x) / 2,std::abs(boxMax.y - boxMin.y) / 2,std::abs(boxMax.z - boxMin.z) / 2};
	}

	uint32_t cullDataStride = sizeof(CullData);
	uint32_t cullDataByteSize = mCullData.size() * cullDataStride;
	LoadResource(
		mCullDataBuffer,
		reinterpret_cast<void*>(mCullData.data()),
		cullDataByteSize,
		mCullData.size(),
		cullDataStride,
		mCullDataSrvAllocInfo.get(),
		CULLDATA_IDX);
}

void Carol::Mesh::LoadResource(
	unique_ptr<DefaultResource>& resource,
	void* data,
	uint32_t size,
	uint32_t numElements,
	uint32_t stride,
	DescriptorAllocInfo* gpuInfo,
	uint32_t srvIdx)
{
	resource = make_unique<DefaultResource>(GetRvaluePtr(CD3DX12_RESOURCE_DESC::Buffer(size)), mHeap);
	resource->CopySubresources(mCommandList, mUploadHeap, GetRvaluePtr(CreateSingleSubresource(data, size)));

	DescriptorAllocInfo cpuInfo;
	mAllocator->CpuAllocate(1, &cpuInfo);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = numElements;
	srvDesc.Buffer.StructureByteStride = stride;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	mDevice->CreateShaderResourceView(resource->Get(), &srvDesc, mAllocator->GetCpuHandle(&cpuInfo));
	mAllocator->GpuAllocate(1, gpuInfo);
	mDevice->CopyDescriptorsSimple(1, mAllocator->GetShaderCpuHandle(gpuInfo), mAllocator->GetCpuHandle(&cpuInfo), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	mMeshIdx[srvIdx] = gpuInfo->StartOffset;
}

Carol::Model::Model(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap, CbvSrvUavDescriptorAllocator* allocator)
	:mDevice(device), mCommandList(cmdList), mHeap(heap), mUploadHeap(uploadHeap), mAllocator(allocator), mSkinnedConstants(make_unique<SkinnedConstants>()), mSkinnedCBAllocInfo(make_unique<HeapAllocInfo>())
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

Carol::vector<int>& Carol::Model::GetBoneHierarchy()
{
	return mBoneHierarchy;
}

Carol::vector<Carol::XMFLOAT4X4>& Carol::Model::GetBoneOffsets()
{
	return mBoneOffsets;
}

const Carol::vector<Carol::vector<Carol::vector<Carol::XMFLOAT4X4>>>& Carol::Model::GetAnimationFrames()
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

void Carol::Model::UpdateAnimationClip(Timer& timer, CircularHeap* skinnedCBHeap)
{
	mTimePos += timer.DeltaTime();

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

		XMStoreFloat4x4(&finalTransforms[i], XMMatrixTranspose(offset * toRoot));
	}

	std::copy(std::begin(mSkinnedConstants->FinalTransforms), std::end(mSkinnedConstants->FinalTransforms), mSkinnedConstants->HistFinalTransforms);
	std::copy(std::begin(finalTransforms), std::end(finalTransforms), mSkinnedConstants->FinalTransforms);

	skinnedCBHeap->DeleteResource(mSkinnedCBAllocInfo.get());
	skinnedCBHeap->CreateResource(mSkinnedCBAllocInfo.get());
	skinnedCBHeap->CopyData(mSkinnedCBAllocInfo.get(), mSkinnedConstants.get());

	mSkinnedCBGPUVirtualAddress = skinnedCBHeap->GetGPUVirtualAddress(mSkinnedCBAllocInfo.get());
}

D3D12_GPU_VIRTUAL_ADDRESS Carol::Model::GetSkinnedCBGPUVirtualAddress()
{
	return mSkinnedCBGPUVirtualAddress;
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
	mMeshes[L"Ground"] = make_unique<Mesh>(this, mDevice, mCommandList, mHeap, mUploadHeap, mAllocator, vertices, indices, false, false);
	mMeshes[L"Ground"]->SetTexIdx(Mesh::DIFFUSE_IDX, texManager->LoadTexture(L"texture\\tile.dds"));
	mMeshes[L"Ground"]->SetTexIdx(Mesh::NORMAL_IDX, texManager->LoadTexture(L"texture\\tile_nmap.dds"));
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
	mMeshes[L"SkyBox"] = make_unique<Mesh>(this, mDevice, mCommandList, mHeap, mUploadHeap, mAllocator, vertices, indices, false, false);
	mMeshes[L"SkyBox"]->SetTexIdx(Mesh::DIFFUSE_IDX, texManager->LoadTexture(L"texture\\snowcube1024.dds"));
}
