#include <render/mesh.h>
#include <render/global_resources.h>
#include <scene/assimp.h>
#include <scene/texture.h>
#include <scene/light.h>
#include <dx12/heap.h>
#include <dx12/descriptor_allocator.h>
#include <dx12/root_signature.h>

namespace Carol {
	using std::wstring;
	using std::make_unique;
	using namespace DirectX;
}

Carol::MeshPass::MeshPass(
	GlobalResources* globalResources,
	bool isSkinned,
	HeapAllocInfo* skinnedCBAllocInfo,
	bool isTransparent,
	uint32_t baseVertexLocation,
	uint32_t startIndexLocation,
	uint32_t indexCount,
	D3D12_VERTEX_BUFFER_VIEW& vertexBufferView,
	D3D12_INDEX_BUFFER_VIEW& indexBufferView)
	:
	Pass(globalResources),
	mSkinned(isSkinned),
	mSkinnedCBAllocInfo(skinnedCBAllocInfo),
	mTransparent(isTransparent),
	mBaseVertexLocation(baseVertexLocation),
	mStartIndexLocation(startIndexLocation),
	mIndexCount(indexCount),
	mVertexBufferView(&vertexBufferView),
	mIndexBufferView(&indexBufferView)
{
	mMeshConstants = make_unique<MeshConstants>();
	mMeshCBAllocInfo = make_unique<HeapAllocInfo>();
	mGlobalResources->CbvSrvUavAllocator->CpuAllocate(TEXTURE_TYPE_COUNT, mTex2DSrvAllocInfo.get());
}

D3D12_VERTEX_BUFFER_VIEW Carol::MeshPass::GetVertexBufferView()
{
	return *mVertexBufferView;
}

D3D12_INDEX_BUFFER_VIEW Carol::MeshPass::GetIndexBufferView()
{
	return *mIndexBufferView;
}

Carol::MeshConstants& Carol::MeshPass::GetMeshConstants()
{
	return *mMeshConstants;
}

void Carol::MeshPass::LoadDiffuseMap(wstring path)
{
	mDiffuseMap = make_unique<Texture>();
	mDiffuseMap->LoadTexture(mGlobalResources->CommandList, mGlobalResources->TexturesHeap, mGlobalResources->UploadBuffersHeap, path);

	mGlobalResources->Device->CreateShaderResourceView(mDiffuseMap->GetResource()->Get(), GetRvaluePtr(mDiffuseMap->GetDesc()), GetTex2DSrv(DIFFUSE_TEXTURE));
}

void Carol::MeshPass::LoadNormalMap(wstring path)
{
	mNormalMap = make_unique<Texture>();
	mNormalMap->LoadTexture(mGlobalResources->CommandList, mGlobalResources->TexturesHeap, mGlobalResources->UploadBuffersHeap, path);

	mGlobalResources->Device->CreateShaderResourceView(mNormalMap->GetResource()->Get(), GetRvaluePtr(mNormalMap->GetDesc()), GetTex2DSrv(NORMAL_TEXTURE));
}

void Carol::MeshPass::SetWorld(XMMATRIX world)
{
	XMStoreFloat4x4(&mMeshConstants->World, XMMatrixTranspose(world));
	XMStoreFloat4x4(&mMeshConstants->HistWorld, XMMatrixTranspose(world));
	TransformBoundingBox(world);
}

void Carol::MeshPass::SetBoundingBox(aiAABB* boundingBox)
{
	DirectX::XMVECTOR boxMin = { boundingBox->mMin.x,boundingBox->mMin.y, boundingBox->mMin.z };
	DirectX::XMVECTOR boxMax = { boundingBox->mMax.x,boundingBox->mMax.y, boundingBox->mMax.z };

	DirectX::XMFLOAT3 boxCenter;
	DirectX::XMFLOAT3 boxExtent;
	
	XMStoreFloat3(&mBoxMin, boxMin);
	XMStoreFloat3(&mBoxMax, boxMax);
	XMStoreFloat3(&boxCenter, (boxMin + boxMax) / 2.0f);
	XMStoreFloat3(&boxExtent, (boxMax - boxMin) / 2.0f);

	mBoundingBox = { boxCenter, boxExtent };
}

void Carol::MeshPass::SetBoundingBox(DirectX::XMVECTOR boxMin, DirectX::XMVECTOR boxMax)
{
	DirectX::XMFLOAT3 boxCenter;
	DirectX::XMFLOAT3 boxExtent;
	
	XMStoreFloat3(&mBoxMin, boxMin);
	XMStoreFloat3(&mBoxMax, boxMax);
	XMStoreFloat3(&boxCenter, (boxMin + boxMax) / 2.0f);
	XMStoreFloat3(&boxExtent, (boxMax - boxMin) / 2.0f);

	mBoundingBox = { boxCenter, boxExtent };
}

void Carol::MeshPass::TransformBoundingBox(DirectX::XMMATRIX transform)
{
	auto boxMin = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&mBoxMin), transform);
	auto boxMax = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&mBoxMax), transform);
	
	DirectX::XMFLOAT3 boxCenter;
	DirectX::XMFLOAT3 boxExtent;

	XMStoreFloat3(&boxCenter, (boxMin + boxMax) / 2.0f);
	XMStoreFloat3(&boxExtent, (boxMax - boxMin) / 2.0f);

	mBoundingBox = { boxCenter, boxExtent };
}

DirectX::BoundingBox Carol::MeshPass::GetBoundingBox()
{
	return mBoundingBox;
}

bool Carol::MeshPass::IsSkinned()
{
	return mSkinned;
}

bool Carol::MeshPass::IsTransparent()
{
	return mTransparent;
}

void Carol::MeshPass::CopyDescriptors()
{
	Pass::CopyDescriptors();

	mMeshConstants->MeshDiffuseMapIdx = mTex2DGpuSrvStartOffset + DIFFUSE_TEXTURE;
	mMeshConstants->MeshNormalMapIdx = mTex2DGpuSrvStartOffset + NORMAL_TEXTURE;
}

void Carol::MeshPass::InitShaders()
{
}

void Carol::MeshPass::InitPSOs()
{
}

void Carol::MeshPass::Draw()
{
	mGlobalResources->CommandList->IASetVertexBuffers(0, 1, mVertexBufferView);
	mGlobalResources->CommandList->IASetIndexBuffer(mIndexBufferView);
	mGlobalResources->CommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mGlobalResources->CommandList->SetGraphicsRootConstantBufferView(RootSignature::ROOT_SIGNATURE_MESH_CB, MeshCBHeap->GetGPUVirtualAddress(mMeshCBAllocInfo.get()));

	if (mSkinnedCBAllocInfo)
	{
		mGlobalResources->CommandList->SetGraphicsRootConstantBufferView(RootSignature::ROOT_SIGNATURE_SKINNED_CB, AssimpModel::GetSkinnedCBHeap()->GetGPUVirtualAddress(mSkinnedCBAllocInfo));
	}

	mGlobalResources->CommandList->DrawIndexedInstanced(mIndexCount, 1, mStartIndexLocation, mBaseVertexLocation, 0);
}

void Carol::MeshPass::Update()
{
	CopyDescriptors();

	MeshCBHeap->DeleteResource(mMeshCBAllocInfo.get());
	MeshCBHeap->CreateResource(nullptr, nullptr, mMeshCBAllocInfo.get());
	MeshCBHeap->CopyData(mMeshCBAllocInfo.get(), mMeshConstants.get());
}

void Carol::MeshPass::OnResize()
{
}

void Carol::MeshPass::ReleaseIntermediateBuffers()
{
	if (mDiffuseMap)
	{
		mDiffuseMap->ReleaseIntermediateBuffer();
	}

	if (mNormalMap)
	{
		mNormalMap->ReleaseIntermediateBuffer();
	}
}

void Carol::MeshPass::InitMeshCBHeap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
	if (!MeshCBHeap)
	{
		MeshCBHeap = make_unique<CircularHeap>(device, cmdList, true, 2048, sizeof(MeshConstants));
	}
}

void Carol::MeshPass::InitResources()
{
}

void Carol::MeshPass::InitDescriptors()
{
}
