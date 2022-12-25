#include <render_pass/render_pass.h>
#include <render_pass/global_resources.h>
#include <dx12/descriptor_allocator.h>
#include <dx12/root_signature.h>

namespace Carol
{
	using std::vector;
	using std::unique_ptr;
	using std::make_unique;
}

Carol::RenderPass::RenderPass(GlobalResources* globalResources)
	:
	mGlobalResources(globalResources),
	mCpuCbvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mGpuCbvAllocInfo(globalResources->NumFrame),
	mCpuSrvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mGpuSrvAllocInfo(globalResources->NumFrame),
	mCpuUavAllocInfo(make_unique<DescriptorAllocInfo>()),
	mGpuUavAllocInfo(globalResources->NumFrame),
	mRtvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mDsvAllocInfo(make_unique<DescriptorAllocInfo>())
{
	for (int i = 0; i < mGlobalResources->NumFrame; ++i)
	{
		mGpuCbvAllocInfo[i] = make_unique<DescriptorAllocInfo>();
		mGpuSrvAllocInfo[i] = make_unique<DescriptorAllocInfo>();
		mGpuUavAllocInfo[i] = make_unique<DescriptorAllocInfo>();
	}
}

Carol::RenderPass::~RenderPass()
{
	if (mCpuCbvAllocInfo->Allocator)
	{
		mCpuCbvAllocInfo->Allocator->CpuDeallocate(mCpuCbvAllocInfo.get());
	}	

	if (mCpuSrvAllocInfo->Allocator)
	{
		mCpuSrvAllocInfo->Allocator->CpuDeallocate(mCpuSrvAllocInfo.get());
	}
	
	if (mCpuUavAllocInfo->Allocator)
	{
		mCpuUavAllocInfo->Allocator->CpuDeallocate(mCpuUavAllocInfo.get());
	}

	if (mRtvAllocInfo->Allocator)
	{
		mRtvAllocInfo->Allocator->CpuDeallocate(mRtvAllocInfo.get());
	}

	if (mDsvAllocInfo->Allocator)
	{
		mDsvAllocInfo->Allocator->CpuDeallocate(mDsvAllocInfo.get());
	}

	for (int i = 0; i < mGlobalResources->NumFrame; ++i)
	{
		if (mGpuCbvAllocInfo[i]->Allocator)
		{
			mGpuCbvAllocInfo[i]->Allocator->GpuDeallocate(mGpuCbvAllocInfo[i].get());
		}

		if (mGpuSrvAllocInfo[i]->Allocator)
		{
			mGpuSrvAllocInfo[i]->Allocator->GpuDeallocate(mGpuSrvAllocInfo[i].get());
		}

		if (mGpuUavAllocInfo[i]->Allocator)
		{
			mGpuUavAllocInfo[i]->Allocator->GpuDeallocate(mGpuUavAllocInfo[i].get());
		}
	}
}

void Carol::RenderPass::OnResize()
{
	if (mCpuCbvAllocInfo->Allocator)
	{
		mGlobalResources->CbvSrvUavAllocator->CpuDeallocate(mCpuCbvAllocInfo.get());
	}

	if (mCpuSrvAllocInfo->Allocator)
	{
		mGlobalResources->CbvSrvUavAllocator->CpuDeallocate(mCpuSrvAllocInfo.get());
	}

	if (mCpuUavAllocInfo->Allocator)
	{
		mGlobalResources->CbvSrvUavAllocator->CpuDeallocate(mCpuUavAllocInfo.get());
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

void Carol::RenderPass::CopyDescriptors()
{
	CopyCbvDescriptors();
	CopySrvDescriptors();
	CopyUavDescriptors();
}

void Carol::RenderPass::CopyCbvDescriptors()
{
	if (mCpuCbvAllocInfo->NumDescriptors == 0)
	{
		return;
	}

	uint32_t idx = *mGlobalResources->CurrFrame;

	if (mGpuCbvAllocInfo[idx]->Allocator)
	{
		mGlobalResources->CbvSrvUavAllocator->GpuDeallocate(mGpuCbvAllocInfo[idx].get());
	}

	mGlobalResources->CbvSrvUavAllocator->GpuAllocate(mCpuCbvAllocInfo->NumDescriptors, mGpuCbvAllocInfo[idx].get());

	auto cpuCbv = GetCpuCbv(0);
	auto shaderCpuCbv = GetShaderCpuCbv(0);

	mGlobalResources->Device->CopyDescriptorsSimple(mCpuCbvAllocInfo->NumDescriptors, shaderCpuCbv, cpuCbv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void Carol::RenderPass::CopySrvDescriptors()
{
	if (mCpuSrvAllocInfo->NumDescriptors == 0)
	{
		return;
	}

	uint32_t idx = *mGlobalResources->CurrFrame;

	if (mGpuSrvAllocInfo[idx]->Allocator)
	{
		mGlobalResources->CbvSrvUavAllocator->GpuDeallocate(mGpuSrvAllocInfo[idx].get());
	}

	mGlobalResources->CbvSrvUavAllocator->GpuAllocate(mCpuSrvAllocInfo->NumDescriptors, mGpuSrvAllocInfo[idx].get());

	auto cpuSrv = GetCpuSrv(0);
	auto shaderCpuSrv = GetShaderCpuSrv(0);

	mGlobalResources->Device->CopyDescriptorsSimple(mCpuSrvAllocInfo->NumDescriptors, shaderCpuSrv, cpuSrv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void Carol::RenderPass::CopyUavDescriptors()
{
	if (mCpuUavAllocInfo->NumDescriptors == 0)
	{
		return;
	}

	uint32_t idx = *mGlobalResources->CurrFrame;

	if (mGpuUavAllocInfo[idx]->Allocator)
	{
		mGlobalResources->CbvSrvUavAllocator->GpuDeallocate(mGpuUavAllocInfo[idx].get());
	}

	mGlobalResources->CbvSrvUavAllocator->GpuAllocate(mCpuUavAllocInfo->NumDescriptors, mGpuUavAllocInfo[idx].get());

	auto cpuUav = GetCpuUav(0);
	auto shaderCpuUav = GetShaderCpuUav(0);

	mGlobalResources->Device->CopyDescriptorsSimple(mCpuUavAllocInfo->NumDescriptors, shaderCpuUav, cpuUav, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::RenderPass::GetCpuCbv(int idx)
{
	auto cpuCbv = mGlobalResources->CbvSrvUavAllocator->GetCpuHandle(mCpuCbvAllocInfo.get());
	cpuCbv.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return cpuCbv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::RenderPass::GetShaderCpuCbv(int idx)
{
	auto shaderCpuCbv = mGlobalResources->CbvSrvUavAllocator->GetShaderCpuHandle(mGpuCbvAllocInfo[*mGlobalResources->CurrFrame].get());
	shaderCpuCbv.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderCpuCbv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::RenderPass::GetShaderGpuCbv(int idx)
{
	auto shaderGpuCbv = mGlobalResources->CbvSrvUavAllocator->GetShaderGpuHandle(mGpuCbvAllocInfo[*mGlobalResources->CurrFrame].get());
	shaderGpuCbv.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderGpuCbv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::RenderPass::GetCpuSrv(int idx)
{
	auto cpuSrv = mGlobalResources->CbvSrvUavAllocator->GetCpuHandle(mCpuSrvAllocInfo.get());
	cpuSrv.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return cpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::RenderPass::GetShaderCpuSrv(int idx)
{
	auto shaderCpuSrv = mGlobalResources->CbvSrvUavAllocator->GetShaderCpuHandle(mGpuSrvAllocInfo[*mGlobalResources->CurrFrame].get());
	shaderCpuSrv.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderCpuSrv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::RenderPass::GetShaderGpuSrv(int idx)
{
	auto shaderGpuSrv = mGlobalResources->CbvSrvUavAllocator->GetShaderGpuHandle(mGpuSrvAllocInfo[*mGlobalResources->CurrFrame].get());
	shaderGpuSrv.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::RenderPass::GetCpuUav(int idx)
{
	auto uav = mGlobalResources->CbvSrvUavAllocator->GetCpuHandle(mCpuUavAllocInfo.get());
	uav.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return uav;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::RenderPass::GetShaderCpuUav(int idx)
{
	auto shaderCpuUav = mGlobalResources->CbvSrvUavAllocator->GetShaderCpuHandle(mGpuUavAllocInfo[*mGlobalResources->CurrFrame].get());
	shaderCpuUav.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderCpuUav;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::RenderPass::GetShaderGpuUav(int idx)
{
	auto shaderGpuUav = mGlobalResources->CbvSrvUavAllocator->GetShaderGpuHandle(mGpuUavAllocInfo[*mGlobalResources->CurrFrame].get());
	shaderGpuUav.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderGpuUav;
}

