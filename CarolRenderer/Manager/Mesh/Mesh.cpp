#include "Mesh.h"
#include "../Light/Light.h"
#include "../Ambient Occlusion/Ssao.h"
#include "../../DirectX/Heap.h"
#include "../../Resource/AssimpModel.h"
#include "../../Resource/Texture.h"

using std::make_unique;

Carol::MeshManager::MeshManager(
	RenderData* renderData,
	bool isSkinned,
	HeapAllocInfo* skinnedCBAllocInfo,
	bool isTransparent,
	uint32_t baseVertexLocation,
	uint32_t startIndexLocation,
	uint32_t indexCount,
	D3D12_VERTEX_BUFFER_VIEW& vertexBufferView,
	D3D12_INDEX_BUFFER_VIEW& indexBufferView)
	:
	Manager(renderData),
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
	mRenderData->CbvSrvUavAllocator->CpuAllocate(TEXTURE_TYPE_COUNT, mCpuCbvSrvUavAllocInfo.get());
}

D3D12_VERTEX_BUFFER_VIEW Carol::MeshManager::GetVertexBufferView()
{
	return *mVertexBufferView;
}

D3D12_INDEX_BUFFER_VIEW Carol::MeshManager::GetIndexBufferView()
{
	return *mIndexBufferView;
}

Carol::MeshConstants& Carol::MeshManager::GetMeshConstants()
{
	return *mMeshConstants;
}

void Carol::MeshManager::LoadDiffuseMap(wstring path)
{
	mDiffuseMap = make_unique<Texture>();
	mDiffuseMap->LoadTexture(mRenderData, path);

	mRenderData->Device->CreateShaderResourceView(mDiffuseMap->GetBuffer()->Get(), GetRvaluePtr(mDiffuseMap->GetDesc()), GetCpuSrv(DIFFUSE_TEXTURE));
}

void Carol::MeshManager::LoadNormalMap(wstring path)
{
	mNormalMap = make_unique<Texture>();
	mNormalMap->LoadTexture(mRenderData, path);

	mRenderData->Device->CreateShaderResourceView(mNormalMap->GetBuffer()->Get(), GetRvaluePtr(mNormalMap->GetDesc()), GetCpuSrv(NORMAL_TEXTURE));
}

void Carol::MeshManager::SetWorld(XMMATRIX world)
{
	XMStoreFloat4x4(&mMeshConstants->World, XMMatrixTranspose(world));
	XMStoreFloat4x4(&mMeshConstants->HistWorld, XMMatrixTranspose(world));
	TransformBoundingBox(world);
}

void Carol::MeshManager::SetBoundingBox(aiAABB* boundingBox)
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

void Carol::MeshManager::SetBoundingBox(DirectX::XMVECTOR boxMin, DirectX::XMVECTOR boxMax)
{
	DirectX::XMFLOAT3 boxCenter;
	DirectX::XMFLOAT3 boxExtent;
	
	XMStoreFloat3(&mBoxMin, boxMin);
	XMStoreFloat3(&mBoxMax, boxMax);
	XMStoreFloat3(&boxCenter, (boxMin + boxMax) / 2.0f);
	XMStoreFloat3(&boxExtent, (boxMax - boxMin) / 2.0f);

	mBoundingBox = { boxCenter, boxExtent };
}

void Carol::MeshManager::TransformBoundingBox(DirectX::XMMATRIX transform)
{
	auto boxMin = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&mBoxMin), transform);
	auto boxMax = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&mBoxMax), transform);
	
	DirectX::XMFLOAT3 boxCenter;
	DirectX::XMFLOAT3 boxExtent;

	XMStoreFloat3(&boxCenter, (boxMin + boxMax) / 2.0f);
	XMStoreFloat3(&boxExtent, (boxMax - boxMin) / 2.0f);

	mBoundingBox = { boxCenter, boxExtent };
}

DirectX::BoundingBox Carol::MeshManager::GetBoundingBox()
{
	return mBoundingBox;
}

void Carol::MeshManager::SetTextureDrawing(bool drawing)
{
	mTextureDrawing = drawing;
}

bool Carol::MeshManager::IsSkinned()
{
	return mSkinned;
}

bool Carol::MeshManager::IsTransparent()
{
	return mTransparent;
}

void Carol::MeshManager::InitRootSignature()
{
}

void Carol::MeshManager::InitShaders()
{
}

void Carol::MeshManager::InitPSOs()
{
}

void Carol::MeshManager::Draw()
{
	mRenderData->CommandList->IASetVertexBuffers(0, 1, mVertexBufferView);
	mRenderData->CommandList->IASetIndexBuffer(mIndexBufferView);
	mRenderData->CommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mRenderData->CommandList->SetGraphicsRootConstantBufferView(1, MeshCBHeap->GetGPUVirtualAddress(mMeshCBAllocInfo.get()));

	if (mSkinnedCBAllocInfo)
	{
		mRenderData->CommandList->SetGraphicsRootConstantBufferView(3, AssimpModel::GetSkinnedCBHeap()->GetGPUVirtualAddress(mSkinnedCBAllocInfo));
	}

	if (mTextureDrawing)
	{
		mRenderData->CommandList->SetGraphicsRootDescriptorTable(4, GetShaderGpuSrv(0));
		mRenderData->CommandList->SetGraphicsRootDescriptorTable(5, mRenderData->MainLight->GetShadowSrv());
		mRenderData->CommandList->SetGraphicsRootDescriptorTable(6, mRenderData->Ssao->GetSsaoSrv());
	}

	mRenderData->CommandList->DrawIndexedInstanced(mIndexCount, 1, mStartIndexLocation, mBaseVertexLocation, 0);
}

void Carol::MeshManager::Update()
{
	MeshCBHeap->DeleteResource(mMeshCBAllocInfo.get());
	MeshCBHeap->CreateResource(nullptr, nullptr, mMeshCBAllocInfo.get());
	MeshCBHeap->CopyData(mMeshCBAllocInfo.get(), mMeshConstants.get());
}

void Carol::MeshManager::OnResize()
{
}

void Carol::MeshManager::ReleaseIntermediateBuffers()
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

void Carol::MeshManager::InitMeshCBHeap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
	if (!MeshCBHeap)
	{
		MeshCBHeap = make_unique<CircularHeap>(device, cmdList, true, 2048, sizeof(MeshConstants));
	}
}

void Carol::MeshManager::InitResources()
{
}

void Carol::MeshManager::InitDescriptors()
{
}
