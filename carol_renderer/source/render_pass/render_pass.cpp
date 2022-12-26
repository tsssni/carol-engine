#include <render_pass/render_pass.h>
#include <render_pass/global_resources.h>
#include <dx12/descriptor_allocator.h>
#include <dx12/root_signature.h>

#define MAX_SRV_DIM D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE

namespace Carol
{
	using std::vector;
	using std::unique_ptr;
	using std::make_unique;
}

Carol::RenderPass::RenderPass(GlobalResources* globalResources)
	:
	mGlobalResources(globalResources),
	mRtvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mDsvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mCbvSrvUavAllocInfo(make_unique<DescriptorAllocInfo>())
{
}

Carol::RenderPass::~RenderPass()
{
	if (mCbvSrvUavAllocInfo->Allocator)
	{
		mCbvSrvUavAllocInfo->Allocator->CpuDeallocate(mCbvSrvUavAllocInfo.get());
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

void Carol::RenderPass::OnResize()
{
	if (mRtvAllocInfo->Allocator)
	{
		mGlobalResources->RtvAllocator->CpuDeallocate(mRtvAllocInfo.get());
	}

	if (mDsvAllocInfo->Allocator)
	{
		mGlobalResources->DsvAllocator->CpuDeallocate(mDsvAllocInfo.get());
	}

	if (mCbvSrvUavAllocInfo->Allocator)
	{
		mGlobalResources->CbvSrvUavAllocator->CpuDeallocate(mCbvSrvUavAllocInfo.get());
	}
}

void Carol::RenderPass::CopyDescriptors()
{
	mCbvSrvUavIdx = mGlobalResources->CbvSrvUavAllocator->GpuAllocate(mCbvSrvUavAllocInfo.get());
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::RenderPass::GetRtv(int idx)
{
	auto rtv = mGlobalResources->RtvAllocator->GetCpuHandle(mRtvAllocInfo.get());
	rtv.Offset(idx * mGlobalResources->RtvAllocator->GetDescriptorSize());

	return rtv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::RenderPass::GetDsv(int idx)
{
	auto dsv = mGlobalResources->DsvAllocator->GetCpuHandle(mDsvAllocInfo.get());
	dsv.Offset(idx * mGlobalResources->DsvAllocator->GetDescriptorSize());

	return dsv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::RenderPass::GetCpuCbvSrvUav(int idx)
{
	auto cbvSrvUav = mGlobalResources->CbvSrvUavAllocator->GetCpuHandle(mCbvSrvUavAllocInfo.get());
	cbvSrvUav.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return cbvSrvUav;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::RenderPass::GetGpuCbvSrvUav(int idx)
{
	return mGlobalResources->CbvSrvUavAllocator->GetGpuHandle(mCbvSrvUavIdx + idx);
}
