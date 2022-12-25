#include <scene/model.h>
#include <dx12/resource.h>
#include <dx12/heap.h>
#include <scene/skinned_data.h>
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

Carol::Mesh::Mesh(
	bool isTransparent, uint32_t baseVertexLocation, uint32_t startIndexLocation, uint32_t indexCount)
	:mTransparent(isTransparent),
	 mBaseVertexLocation(baseVertexLocation),
	 mStartIndexLocation(startIndexLocation),
	 mIndexCount(indexCount)
{
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

std::wstring Carol::Mesh::GetDiffuseMapPath()
{
	return mDiffuseMapPath;
}

std::wstring Carol::Mesh::GetNormalMapPath()
{
	return mNormalMapPath;
}

void Carol::Mesh::SetMaterial(const Material& mat)
{
	mMaterial = mat;
}

void Carol::Mesh::SetDiffuseMapPath(const wstring& path)
{
	mDiffuseMapPath = path;
}

void Carol::Mesh::SetNormalMapPath(const wstring& path)
{
	mNormalMapPath = path;
}

void Carol::Mesh::SetBoundingBox(DirectX::XMVECTOR boxMin, DirectX::XMVECTOR boxMax)
{
	XMFLOAT3 boxCenter;
	XMFLOAT3 boxExtent;
	
	XMStoreFloat3(&mBoxMin, boxMin);
	XMStoreFloat3(&mBoxMax, boxMax);
	XMStoreFloat3(&boxCenter, (boxMin + boxMax) / 2.0f);
	XMStoreFloat3(&boxExtent, (boxMax - boxMin) / 2.0f);

	mBoundingBox = { boxCenter, boxExtent };
}

void Carol::Mesh::TransformBoundingBox(DirectX::XMMATRIX transform)
{
	auto boxMin = XMVector3Transform(DirectX::XMLoadFloat3(&mBoxMin), transform);
	auto boxMax = XMVector3Transform(DirectX::XMLoadFloat3(&mBoxMax), transform);
	
	XMFLOAT3 boxCenter;
	XMFLOAT3 boxExtent;

	XMStoreFloat3(&boxCenter, (boxMin + boxMax) / 2.0f);
	XMStoreFloat3(&boxExtent, (boxMax - boxMin) / 2.0f);

	mBoundingBox = { boxCenter, boxExtent };
}

Carol::BoundingBox Carol::Mesh::GetBoundingBox()
{
	return mBoundingBox;
}

bool Carol::Mesh::IsTransparent()
{
	return mTransparent;
}

Carol::Model::Model(bool isSkinned)
	:mSkinned(isSkinned)
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

D3D12_VERTEX_BUFFER_VIEW* Carol::Model::GetVertexBufferView()
{
	return &mVertexBufferView;
}

D3D12_INDEX_BUFFER_VIEW* Carol::Model::GetIndexBufferView()
{
	return &mIndexBufferView;
}

const Carol::unordered_map<Carol::wstring, Carol::unique_ptr<Carol::Mesh>>& Carol::Model::GetMeshes()
{
	return mMeshes;
}

Carol::AnimationClip* Carol::Model::GetAnimationClip(wstring clipName)
{
	return mAnimationClips[clipName].get();
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

void Carol::Model::LoadGround(ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap)
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

	mMeshes[L"Ground"] = make_unique<Mesh>(false, 0, 0, 6);
	mMeshes[L"Ground"]->SetDiffuseMapPath(L"texture\\tile.dds");
	mMeshes[L"Ground"]->SetNormalMapPath(L"texture\\tile_nmap.dds");
	mMeshes[L"Ground"]->SetBoundingBox({ -50.0f,-0.1f,-50.0f }, { 50.0f,0.1f,50.0f });
}

void Carol::Model::LoadSkyBox(ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap)
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

	mMeshes[L"SkyBox"] = make_unique<Mesh>(false, 0, 0, 36);
	mMeshes[L"SkyBox"]->SetDiffuseMapPath(L"texture\\snowcube1024.dds");
}
