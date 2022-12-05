#include "Manager.h"
#include "Mesh/Mesh.h"
#include "../DirectX/DescriptorAllocator.h"

using std::make_unique;

void Carol::RenderData::DrawMeshes(ID3D12PipelineState* pso, bool textureDrawing)
{
	CommandList->SetPipelineState(pso);

	for (auto* mesh : *OpaqueStaticMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}

	for (auto* mesh : *OpaqueSkinnedMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}

	for (auto* mesh : *TransparentStaticMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}

	for (auto* mesh : *TransparentSkinnedMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}
}

void Carol::RenderData::DrawMeshes(ID3D12PipelineState* opaqueStaticPSO, ID3D12PipelineState* opaqueSkinnedPSO, ID3D12PipelineState* transparentStaticPSO, ID3D12PipelineState* transparentSkinnedPSO, bool textureDrawing)
{
	CommandList->SetPipelineState(opaqueStaticPSO);
	for (auto* mesh : *OpaqueStaticMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}

	CommandList->SetPipelineState(opaqueSkinnedPSO);
	for (auto* mesh : *OpaqueSkinnedMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}

	CommandList->SetPipelineState(transparentStaticPSO);
	for (auto* mesh : *TransparentStaticMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}

	CommandList->SetPipelineState(transparentSkinnedPSO);
	for (auto* mesh : *TransparentSkinnedMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}
}

void Carol::RenderData::DrawOpaqueMeshes(ID3D12PipelineState* opaqueStaticPSO, ID3D12PipelineState* opaqueSkinnedPSO, bool textureDrawing)
{
	CommandList->SetPipelineState(opaqueStaticPSO);
	for (auto* mesh : *OpaqueStaticMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}

	CommandList->SetPipelineState(opaqueSkinnedPSO);
	for (auto* mesh : *OpaqueSkinnedMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}
}

void Carol::RenderData::DrawTransparentMeshes(ID3D12PipelineState* transparentStaticPSO, ID3D12PipelineState* transparentSkinnedPSO, bool textureDrawing)
{
	CommandList->SetPipelineState(transparentStaticPSO);
	for (auto* mesh : *TransparentStaticMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}

	CommandList->SetPipelineState(transparentSkinnedPSO);
	for (auto* mesh : *TransparentSkinnedMeshes)
	{
		mesh->SetTextureDrawing(textureDrawing);
		mesh->Draw();
	}
}

void Carol::RenderData::DrawSkyBox(ID3D12PipelineState* skyBoxPSO)
{
	CommandList->SetPipelineState(skyBoxPSO);
	SkyBoxMesh->Draw();
}

Carol::Manager::Manager(RenderData* renderData)
	:
	mRenderData(renderData),
	mCpuCbvSrvUavAllocInfo(make_unique<DescriptorAllocInfo>()),
	mGpuCbvSrvUavAllocInfo(make_unique<DescriptorAllocInfo>()),
	mRtvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mDsvAllocInfo(make_unique<DescriptorAllocInfo>())
{
}

Carol::Manager::~Manager()
{
	if (mCpuCbvSrvUavAllocInfo->Allocator)
	{
		mCpuCbvSrvUavAllocInfo->Allocator->CpuDeallocate(mCpuCbvSrvUavAllocInfo.get());
	}

	if (mGpuCbvSrvUavAllocInfo->Allocator)
	{
		mGpuCbvSrvUavAllocInfo->Allocator->GpuDeallocate(mGpuCbvSrvUavAllocInfo.get());
	}

	if (mRtvAllocInfo->Allocator)
	{
		mRtvAllocInfo->Allocator->CpuDeallocate(mRtvAllocInfo.get());
	}

	if (mDsvAllocInfo->Allocator)
	{
		mDsvAllocInfo->Allocator->CpuDeallocate(mDsvAllocInfo.get());
	}
}

void Carol::Manager::OnResize()
{
	if (mCpuCbvSrvUavAllocInfo->Allocator)
	{
		mRenderData->CbvSrvUavAllocator->CpuDeallocate(mCpuCbvSrvUavAllocInfo.get());
	}

	if (mRtvAllocInfo->Allocator)
	{
		mRenderData->RtvAllocator->CpuDeallocate(mRtvAllocInfo.get());
	}

	if (mDsvAllocInfo->Allocator)
	{
		mRenderData->DsvAllocator->CpuDeallocate(mDsvAllocInfo.get());
	}
}

void Carol::Manager::CopyDescriptors()
{
	if (mGpuCbvSrvUavAllocInfo->Allocator)
	{
		mRenderData->CbvSrvUavAllocator->GpuDeallocate(mGpuCbvSrvUavAllocInfo.get());
	}

	mGpuCbvSrvUavAllocInfo = make_unique<DescriptorAllocInfo>();
	mRenderData->CbvSrvUavAllocator->GpuAllocate(mCpuCbvSrvUavAllocInfo->NumDescriptors, mGpuCbvSrvUavAllocInfo.get());

	auto cpuSrv = GetCpuSrv(0);
	auto shaderCpuSrv = GetShaderCpuSrv(0);

	mRenderData->Device->CopyDescriptorsSimple(mCpuCbvSrvUavAllocInfo->NumDescriptors, shaderCpuSrv, cpuSrv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Manager::GetRtv(int idx)
{
	auto rtv = mRenderData->RtvAllocator->GetCpuHandle(mRtvAllocInfo.get());
	rtv.Offset(idx * mRenderData->RtvAllocator->GetDescriptorSize());

	return rtv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Manager::GetDsv(int idx)
{
	auto dsv = mRenderData->DsvAllocator->GetCpuHandle(mDsvAllocInfo.get());
	dsv.Offset(idx * mRenderData->DsvAllocator->GetDescriptorSize());

	return dsv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Manager::GetCbv(int idx)
{
	auto cbv = mRenderData->CbvSrvUavAllocator->GetCpuHandle(mCpuCbvSrvUavAllocInfo.get());
	cbv.Offset(idx * mRenderData->CbvSrvUavAllocator->GetDescriptorSize());

	return cbv;
}


CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Manager::GetCpuSrv(int idx)
{
	auto cpuSrv = mRenderData->CbvSrvUavAllocator->GetCpuHandle(mCpuCbvSrvUavAllocInfo.get());
	cpuSrv.Offset(idx * mRenderData->CbvSrvUavAllocator->GetDescriptorSize());

	return cpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Manager::GetShaderCpuSrv(int idx)
{
	auto shaderCpuSrv = mRenderData->CbvSrvUavAllocator->GetShaderCpuHandle(mGpuCbvSrvUavAllocInfo.get());
	shaderCpuSrv.Offset(idx * mRenderData->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderCpuSrv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::Manager::GetShaderGpuSrv(int idx)
{
	auto shaderGpuSrv = mRenderData->CbvSrvUavAllocator->GetShaderGpuHandle(mGpuCbvSrvUavAllocInfo.get());
	shaderGpuSrv.Offset(idx * mRenderData->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Manager::GetCpuUav(int idx)
{
	auto uav = mRenderData->CbvSrvUavAllocator->GetCpuHandle(mCpuCbvSrvUavAllocInfo.get());
	uav.Offset(idx * mRenderData->CbvSrvUavAllocator->GetDescriptorSize());

	return uav;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Manager::GetShaderCpuUav(int idx)
{
	auto shaderCpuUav = mRenderData->CbvSrvUavAllocator->GetShaderCpuHandle(mGpuCbvSrvUavAllocInfo.get());
	shaderCpuUav.Offset(idx * mRenderData->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderCpuUav;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::Manager::GetShaderGpuUav(int idx)
{
	auto shaderGpuUav = mRenderData->CbvSrvUavAllocator->GetShaderGpuHandle(mGpuCbvSrvUavAllocInfo.get());
	shaderGpuUav.Offset(idx * mRenderData->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderGpuUav;
}

