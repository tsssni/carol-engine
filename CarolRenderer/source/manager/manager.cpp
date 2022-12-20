#include <manager/manager.h>
#include <global_resources.h>
#include <dx12/descriptor_allocator.h>

namespace Carol
{
	using std::make_unique;
}

Carol::Manager::Manager(GlobalResources* globalResources)
	:
	mGlobalResources(globalResources),
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
		mGlobalResources->CbvSrvUavAllocator->CpuDeallocate(mCpuCbvSrvUavAllocInfo.get());
	}

	if (mRtvAllocInfo->Allocator)
	{
		mGlobalResources->RtvAllocator->CpuDeallocate(mRtvAllocInfo.get());
	}

	if (mDsvAllocInfo->Allocator)
	{
		mGlobalResources->DsvAllocator->CpuDeallocate(mDsvAllocInfo.get());
	}
}

void Carol::Manager::CopyDescriptors()
{
	if (mGpuCbvSrvUavAllocInfo->Allocator)
	{
		mGlobalResources->CbvSrvUavAllocator->GpuDeallocate(mGpuCbvSrvUavAllocInfo.get());
	}

	mGpuCbvSrvUavAllocInfo = make_unique<DescriptorAllocInfo>();
	mGlobalResources->CbvSrvUavAllocator->GpuAllocate(mCpuCbvSrvUavAllocInfo->NumDescriptors, mGpuCbvSrvUavAllocInfo.get());

	auto cpuSrv = GetCpuSrv(0);
	auto shaderCpuSrv = GetShaderCpuSrv(0);

	mGlobalResources->Device->CopyDescriptorsSimple(mCpuCbvSrvUavAllocInfo->NumDescriptors, shaderCpuSrv, cpuSrv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Manager::GetRtv(int idx)
{
	auto rtv = mGlobalResources->RtvAllocator->GetCpuHandle(mRtvAllocInfo.get());
	rtv.Offset(idx * mGlobalResources->RtvAllocator->GetDescriptorSize());

	return rtv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Manager::GetDsv(int idx)
{
	auto dsv = mGlobalResources->DsvAllocator->GetCpuHandle(mDsvAllocInfo.get());
	dsv.Offset(idx * mGlobalResources->DsvAllocator->GetDescriptorSize());

	return dsv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Manager::GetCbv(int idx)
{
	auto cbv = mGlobalResources->CbvSrvUavAllocator->GetCpuHandle(mCpuCbvSrvUavAllocInfo.get());
	cbv.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return cbv;
}


CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Manager::GetCpuSrv(int idx)
{
	auto cpuSrv = mGlobalResources->CbvSrvUavAllocator->GetCpuHandle(mCpuCbvSrvUavAllocInfo.get());
	cpuSrv.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return cpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Manager::GetShaderCpuSrv(int idx)
{
	auto shaderCpuSrv = mGlobalResources->CbvSrvUavAllocator->GetShaderCpuHandle(mGpuCbvSrvUavAllocInfo.get());
	shaderCpuSrv.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderCpuSrv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::Manager::GetShaderGpuSrv(int idx)
{
	auto shaderGpuSrv = mGlobalResources->CbvSrvUavAllocator->GetShaderGpuHandle(mGpuCbvSrvUavAllocInfo.get());
	shaderGpuSrv.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Manager::GetCpuUav(int idx)
{
	auto uav = mGlobalResources->CbvSrvUavAllocator->GetCpuHandle(mCpuCbvSrvUavAllocInfo.get());
	uav.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return uav;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Manager::GetShaderCpuUav(int idx)
{
	auto shaderCpuUav = mGlobalResources->CbvSrvUavAllocator->GetShaderCpuHandle(mGpuCbvSrvUavAllocInfo.get());
	shaderCpuUav.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderCpuUav;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::Manager::GetShaderGpuUav(int idx)
{
	auto shaderGpuUav = mGlobalResources->CbvSrvUavAllocator->GetShaderGpuHandle(mGpuCbvSrvUavAllocInfo.get());
	shaderGpuUav.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderGpuUav;
}

