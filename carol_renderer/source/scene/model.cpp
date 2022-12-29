#include <scene/model.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <scene/scene.h>
#include <scene/skinned_data.h>
#include <scene/timer.h>
#include <scene/texture.h>
#include <utils/common.h>

namespace Carol
{
	using std::vector;
	using std::unique_ptr;
	using std::make_unique;
	using std::unordered_map;
	using std::wstring;
	using namespace DirectX;
}

Carol::Mesh::Mesh(Model* model, D3D12_VERTEX_BUFFER_VIEW* vertexBufferView, D3D12_INDEX_BUFFER_VIEW* indexBufferView, uint32_t baseVertexLocation, uint32_t startIndexLocation, uint32_t indexCount, bool isSkinned, bool isTransparent)
	:mModel(model),
	mVertexBufferView(vertexBufferView),
	mIndexBufferView(indexBufferView),
	mBaseVertexLocation(baseVertexLocation),
	mStartIndexLocation(startIndexLocation),
	mIndexCount(indexCount),
	mSkinned(isSkinned),
	mTransparent(isTransparent),
	mTexIdx(TEX_IDX_COUNT)
{
}

Carol::Mesh::~Mesh()
{
	auto itr = mOctreeNode->Meshes.begin();

	for (int i = 0; i < mOctreeNodeIdx; ++i)
	{
		++itr;
	}

	mOctreeNode->Meshes.erase(itr);
}

D3D12_VERTEX_BUFFER_VIEW Carol::Mesh::GetVertexBufferView()
{
	return *mVertexBufferView;
}

D3D12_INDEX_BUFFER_VIEW Carol::Mesh::GetIndexBufferView()
{
	return *mIndexBufferView;
}

D3D12_GPU_VIRTUAL_ADDRESS Carol::Mesh::GetSkinnedCBGPUVirtualAddress()
{
	return mModel->GetSkinnedCBGPUVirtualAddress();
}

uint32_t Carol::Mesh::GetBaseVertexLocation()
{
	return mBaseVertexLocation;
}

uint32_t Carol::Mesh::GetStartIndexLocation()
{
	return mStartIndexLocation;
}

uint32_t Carol::Mesh::GetIndexCount()
{
	return mIndexCount;
}

Carol::Material Carol::Mesh::GetMaterial()
{
	return mMaterial;
}

Carol::vector<uint32_t> Carol::Mesh::GetTexIdx()
{
	return mTexIdx;
}

void Carol::Mesh::SetMaterial(const Material& mat)
{
	mMaterial = mat;
}

void Carol::Mesh::SetTexIdx(uint32_t type, uint32_t idx)
{
	mTexIdx[type] = idx;
}

void Carol::Mesh::SetOctreeNode(OctreeNode* node, uint32_t idx)
{
	mOctreeNode = node;
	mOctreeNodeIdx = idx;
}

void Carol::Mesh::SetBoundingBox(DirectX::XMVECTOR boxMin, DirectX::XMVECTOR boxMax)
{
	XMStoreFloat3(&mBoxMin, boxMin);
	XMStoreFloat3(&mBoxMax, boxMax);
	BoundingBox::CreateFromPoints(mBoundingBox, boxMin, boxMax);
}

void Carol::Mesh::TransformBoundingBox(DirectX::XMMATRIX transform)
{
	auto boxMin = XMVector3Transform(DirectX::XMLoadFloat3(&mBoxMin), transform);
	auto boxMax = XMVector3Transform(DirectX::XMLoadFloat3(&mBoxMax), transform);
	BoundingBox::CreateFromPoints(mBoundingBox, boxMin, boxMax);
}

Carol::BoundingBox Carol::Mesh::GetBoundingBox()
{
	return mBoundingBox;
}

bool Carol::Mesh::IsSkinned()
{
	return mSkinned;
}

bool Carol::Mesh::IsTransparent()
{
	return mTransparent;
}

Carol::Model::Model()
	:mSkinnedConstants(make_unique<SkinnedConstants>()), mSkinnedCBAllocInfo(make_unique<HeapAllocInfo>())
{
}

Carol::Model::~Model()
{
}

void Carol::Model::ReleaseIntermediateBuffers()
{
	mVertexBufferGpu->ReleaseIntermediateBuffer();
	mIndexBufferGpu->ReleaseIntermediateBuffer();
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

void Carol::Model::LoadVerticesAndIndices(ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap)
{
	uint32_t vbByteSize = sizeof(Vertex) * mVertices.size();
	uint32_t ibByteSize = sizeof(uint32_t) * mIndices.size();
	uint32_t vertexStride = sizeof(Vertex);
	DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT;

	mVertexBufferGpu = std::make_unique<DefaultResource>(GetRvaluePtr(CD3DX12_RESOURCE_DESC::Buffer(vbByteSize)), heap);
	mVertexBufferGpu->CopySubresources(cmdList, uploadHeap, GetRvaluePtr(CreateSingleSubresource(reinterpret_cast<void*>(mVertices.data()), vbByteSize)));

	mIndexBufferGpu = std::make_unique<DefaultResource>(GetRvaluePtr(CD3DX12_RESOURCE_DESC::Buffer(ibByteSize)), heap);
	mIndexBufferGpu->CopySubresources(cmdList, uploadHeap, GetRvaluePtr(CreateSingleSubresource(reinterpret_cast<void*>(mIndices.data()), ibByteSize)));

	mVertexBufferView = { mVertexBufferGpu->Get()->GetGPUVirtualAddress(), vbByteSize, vertexStride};
	mIndexBufferView = { mIndexBufferGpu->Get()->GetGPUVirtualAddress(), ibByteSize, indexFormat };
}

bool Carol::Model::IsSkinned()
{
	return mSkinned;
}

void Carol::Model::LoadGround(ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap, TextureManager* texManager)
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

	mVertices.resize(4);
	for (int i = 0; i < 4; ++i)
	{
		mVertices[i] = { pos[i],normal,tangent,texC[i] };
	}
	mIndices = { 0,1,2,0,2,3 };
	LoadVerticesAndIndices(cmdList, heap, uploadHeap);

	mMeshes[L"Ground"] = make_unique<Mesh>(this, &mVertexBufferView, &mIndexBufferView, 0, 0, 6, false, false);
	mMeshes[L"Ground"]->SetTexIdx(Mesh::DIFFUSE_IDX, texManager->LoadTexture(L"texture\\tile.dds"));
	mMeshes[L"Ground"]->SetTexIdx(Mesh::NORMAL_IDX, texManager->LoadTexture(L"texture\\tile_nmap.dds"));
	mMeshes[L"Ground"]->SetBoundingBox({ -50.0f,-0.1f,-50.0f }, { 50.0f,0.1f,50.0f });
}

void Carol::Model::LoadSkyBox(ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap, TextureManager* texManager)
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

	mVertices.resize(8);
	for (int i = 0; i < 8; ++i)
	{
		mVertices[i] = { pos[i] };
	}

	mIndices = { 0,1,2,0,2,3,4,5,1,4,1,0,7,6,5,7,5,4,3,2,6,3,6,7,1,5,6,1,6,2,4,0,3,4,3,7 };
	LoadVerticesAndIndices(cmdList, heap, uploadHeap);

	mMeshes[L"SkyBox"] = make_unique<Mesh>(this, &mVertexBufferView, &mIndexBufferView, 0, 0, 36, false, false);
	mMeshes[L"SkyBox"]->SetTexIdx(Mesh::DIFFUSE_IDX, texManager->LoadTexture(L"texture\\snowcube1024.dds"));
}
