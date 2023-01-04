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
	mCpuCbvSrvUavAllocInfo(make_unique<DescriptorAllocInfo>()),
	mGpuCbvSrvUavAllocInfo(make_unique<DescriptorAllocInfo>())
{
}

Carol::RenderPass::~RenderPass()
{
	DeallocateDescriptors();
}

void Carol::RenderPass::OnResize()
{
	DeallocateDescriptors();
}

void Carol::RenderPass::CopyDescriptors()
{
	mGlobalResources->CbvSrvUavAllocator->GpuAllocate(mCpuCbvSrvUavAllocInfo->NumDescriptors, mGpuCbvSrvUavAllocInfo.get());

	uint32_t num = mCpuCbvSrvUavAllocInfo->NumDescriptors;
	auto cpuHandle = mGlobalResources->CbvSrvUavAllocator->GetCpuHandle(mCpuCbvSrvUavAllocInfo.get());
	auto shaderCpuHandle = mGlobalResources->CbvSrvUavAllocator->GetShaderCpuHandle(mGpuCbvSrvUavAllocInfo.get());
	
	mGlobalResources->Device->CopyDescriptorsSimple(num, shaderCpuHandle, cpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void Carol::RenderPass::DeallocateDescriptors()
{
	if (mRtvAllocInfo->Allocator)
	{
		mGlobalResources->RtvAllocator->CpuDeallocate(mRtvAllocInfo.get());
	}

	if (mDsvAllocInfo->Allocator)
	{
		mGlobalResources->DsvAllocator->CpuDeallocate(mDsvAllocInfo.get());
	}

	if (mCpuCbvSrvUavAllocInfo->Allocator)
	{
		mGlobalResources->CbvSrvUavAllocator->CpuDeallocate(mCpuCbvSrvUavAllocInfo.get());
	}

	if (mGpuCbvSrvUavAllocInfo->Allocator)
	{
		mGlobalResources->CbvSrvUavAllocator->GpuDeallocate(mGpuCbvSrvUavAllocInfo.get());
	}

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
	auto cpuCbvSrvUav = mGlobalResources->CbvSrvUavAllocator->GetCpuHandle(mCpuCbvSrvUavAllocInfo.get());
	cpuCbvSrvUav.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return cpuCbvSrvUav;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::RenderPass::GetGpuCbvSrvUav(int idx)
{
	auto gpuCbvSrvUav = mGlobalResources->CbvSrvUavAllocator->GetShaderGpuHandle(mGpuCbvSrvUavAllocInfo.get());
	gpuCbvSrvUav.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return gpuCbvSrvUav;
}
