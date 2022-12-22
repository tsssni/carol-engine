#include <render/pass.h>
#include <render/global_resources.h>
#include <dx12/descriptor_allocator.h>
#include <dx12/root_signature.h>

namespace Carol
{
	using std::make_unique;
}

Carol::Pass::Pass(GlobalResources* globalResources)
	:
	mGlobalResources(globalResources),
	mCpuCbvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mGpuCbvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mCpuSrvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mGpuSrvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mCpuUavAllocInfo(make_unique<DescriptorAllocInfo>()),
	mGpuUavAllocInfo(make_unique<DescriptorAllocInfo>()),
	mTex1DSrvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mTex2DSrvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mTex3DSrvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mTexCubeSrvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mRtvAllocInfo(make_unique<DescriptorAllocInfo>()),
	mDsvAllocInfo(make_unique<DescriptorAllocInfo>())
{
}

Carol::Pass::~Pass()
{
	if (mCpuCbvAllocInfo->Allocator)
	{
		mCpuCbvAllocInfo->Allocator->CpuDeallocate(mCpuCbvAllocInfo.get());
	}

	if (mGpuCbvAllocInfo->Allocator)
	{
		mGpuCbvAllocInfo->Allocator->GpuDeallocate(mGpuCbvAllocInfo.get());
	}

	if (mCpuSrvAllocInfo->Allocator)
	{
		mCpuSrvAllocInfo->Allocator->CpuDeallocate(mCpuSrvAllocInfo.get());
	}

	if (mGpuSrvAllocInfo->Allocator)
	{
		mGpuSrvAllocInfo->Allocator->GpuDeallocate(mGpuSrvAllocInfo.get());
	}

	if (mCpuUavAllocInfo->Allocator)
	{
		mCpuUavAllocInfo->Allocator->CpuDeallocate(mCpuUavAllocInfo.get());
	}

	if (mGpuUavAllocInfo->Allocator)
	{
		mGpuUavAllocInfo->Allocator->GpuDeallocate(mGpuUavAllocInfo.get());
	}
	
	if (mTex1DSrvAllocInfo->Allocator)
	{
		mTex1DSrvAllocInfo->Allocator->CpuDeallocate(mTex1DSrvAllocInfo.get());
	}

	if (mTex2DSrvAllocInfo->Allocator)
	{
		mTex2DSrvAllocInfo->Allocator->CpuDeallocate(mTex2DSrvAllocInfo.get());
	}

	if (mTex3DSrvAllocInfo->Allocator)
	{
		mTex3DSrvAllocInfo->Allocator->CpuDeallocate(mTex3DSrvAllocInfo.get());
	}

	if (mTexCubeSrvAllocInfo->Allocator)
	{
		mTexCubeSrvAllocInfo->Allocator->CpuDeallocate(mTexCubeSrvAllocInfo.get());
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

void Carol::Pass::OnResize()
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

	if (mTex1DSrvAllocInfo->Allocator)
	{
		mGlobalResources->CbvSrvUavAllocator->CpuDeallocate(mTex1DSrvAllocInfo.get());
	}

	if (mTex2DSrvAllocInfo->Allocator)
	{
		mGlobalResources->CbvSrvUavAllocator->CpuDeallocate(mTex2DSrvAllocInfo.get());
	}

	if (mTex3DSrvAllocInfo->Allocator)
	{
		mGlobalResources->CbvSrvUavAllocator->CpuDeallocate(mTex3DSrvAllocInfo.get());
	}

	if (mTexCubeSrvAllocInfo->Allocator)
	{
		mGlobalResources->CbvSrvUavAllocator->CpuDeallocate(mTexCubeSrvAllocInfo.get());
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

void Carol::Pass::CopyDescriptors()
{
	CopyCbvDescriptors();
	CopySrvDescriptors();
	CopyUavDescriptors();

	mTex1DGpuSrvStartOffset = mGlobalResources->RootSignature->AllocateTex1D(mTex1DSrvAllocInfo.get());
	mTex2DGpuSrvStartOffset = mGlobalResources->RootSignature->AllocateTex2D(mTex2DSrvAllocInfo.get());
	mTex3DGpuSrvStartOffset = mGlobalResources->RootSignature->AllocateTex3D(mTex3DSrvAllocInfo.get());
	mTexCubeGpuSrvStartOffset = mGlobalResources->RootSignature->AllocateTexCube(mTexCubeSrvAllocInfo.get());
}

void Carol::Pass::CopyCbvDescriptors()
{
	if (mGpuCbvAllocInfo->Allocator)
	{
		mGlobalResources->CbvSrvUavAllocator->GpuDeallocate(mGpuCbvAllocInfo.get());
	}

	mGpuCbvAllocInfo = make_unique<DescriptorAllocInfo>();
	mGlobalResources->CbvSrvUavAllocator->GpuAllocate(mCpuCbvAllocInfo->NumDescriptors, mGpuCbvAllocInfo.get());

	auto cpuCbv = GetCpuCbv(0);
	auto shaderCpuCbv = GetShaderCpuCbv(0);

	mGlobalResources->Device->CopyDescriptorsSimple(mCpuCbvAllocInfo->NumDescriptors, shaderCpuCbv, cpuCbv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void Carol::Pass::CopySrvDescriptors()
{
	if (mGpuSrvAllocInfo->Allocator)
	{
		mGlobalResources->CbvSrvUavAllocator->GpuDeallocate(mGpuSrvAllocInfo.get());
	}

	mGpuSrvAllocInfo = make_unique<DescriptorAllocInfo>();
	mGlobalResources->CbvSrvUavAllocator->GpuAllocate(mCpuSrvAllocInfo->NumDescriptors, mGpuSrvAllocInfo.get());

	auto cpuSrv = GetCpuSrv(0);
	auto shaderCpuSrv = GetShaderCpuSrv(0);

	mGlobalResources->Device->CopyDescriptorsSimple(mCpuSrvAllocInfo->NumDescriptors, shaderCpuSrv, cpuSrv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void Carol::Pass::CopyUavDescriptors()
{
	if (mGpuUavAllocInfo->Allocator)
	{
		mGlobalResources->CbvSrvUavAllocator->GpuDeallocate(mGpuUavAllocInfo.get());
	}

	mGpuUavAllocInfo = make_unique<DescriptorAllocInfo>();
	mGlobalResources->CbvSrvUavAllocator->GpuAllocate(mCpuUavAllocInfo->NumDescriptors, mGpuUavAllocInfo.get());

	auto cpuUav = GetCpuUav(0);
	auto shaderCpuUav = GetShaderCpuUav(0);

	mGlobalResources->Device->CopyDescriptorsSimple(mCpuUavAllocInfo->NumDescriptors, shaderCpuUav, cpuUav, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Pass::GetRtv(int idx)
{
	auto rtv = mGlobalResources->RtvAllocator->GetCpuHandle(mRtvAllocInfo.get());
	rtv.Offset(idx * mGlobalResources->RtvAllocator->GetDescriptorSize());

	return rtv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Pass::GetDsv(int idx)
{
	auto dsv = mGlobalResources->DsvAllocator->GetCpuHandle(mDsvAllocInfo.get());
	dsv.Offset(idx * mGlobalResources->DsvAllocator->GetDescriptorSize());

	return dsv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Pass::GetCpuCbv(int idx)
{
	auto cpuCbv = mGlobalResources->CbvSrvUavAllocator->GetCpuHandle(mCpuCbvAllocInfo.get());
	cpuCbv.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return cpuCbv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Pass::GetShaderCpuCbv(int idx)
{
	auto shaderCpuCbv = mGlobalResources->CbvSrvUavAllocator->GetShaderCpuHandle(mGpuCbvAllocInfo.get());
	shaderCpuCbv.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderCpuCbv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::Pass::GetShaderGpuCbv(int idx)
{
	auto shaderGpuCbv = mGlobalResources->CbvSrvUavAllocator->GetShaderGpuHandle(mGpuCbvAllocInfo.get());
	shaderGpuCbv.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderGpuCbv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Pass::GetTex1DSrv(int idx)
{
	auto srvTex1D = mGlobalResources->CbvSrvUavAllocator->GetCpuHandle(mTex1DSrvAllocInfo.get());
	srvTex1D.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return srvTex1D;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Pass::GetTex2DSrv(int idx)
{
	auto srvTex2D = mGlobalResources->CbvSrvUavAllocator->GetCpuHandle(mTex2DSrvAllocInfo.get());
	srvTex2D.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return srvTex2D;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Pass::GetTex3DSrv(int idx)
{
	auto srvTex3D = mGlobalResources->CbvSrvUavAllocator->GetCpuHandle(mTex3DSrvAllocInfo.get());
	srvTex3D.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return srvTex3D;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Pass::GetTexCubeSrv(int idx)
{
	auto srvTexCube = mGlobalResources->CbvSrvUavAllocator->GetCpuHandle(mTexCubeSrvAllocInfo.get());
	srvTexCube.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return srvTexCube;
}


CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Pass::GetCpuSrv(int idx)
{
	auto cpuSrv = mGlobalResources->CbvSrvUavAllocator->GetCpuHandle(mCpuSrvAllocInfo.get());
	cpuSrv.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return cpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Pass::GetShaderCpuSrv(int idx)
{
	auto shaderCpuSrv = mGlobalResources->CbvSrvUavAllocator->GetShaderCpuHandle(mGpuSrvAllocInfo.get());
	shaderCpuSrv.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderCpuSrv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::Pass::GetShaderGpuSrv(int idx)
{
	auto shaderGpuSrv = mGlobalResources->CbvSrvUavAllocator->GetShaderGpuHandle(mGpuSrvAllocInfo.get());
	shaderGpuSrv.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Pass::GetCpuUav(int idx)
{
	auto uav = mGlobalResources->CbvSrvUavAllocator->GetCpuHandle(mCpuUavAllocInfo.get());
	uav.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return uav;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::Pass::GetShaderCpuUav(int idx)
{
	auto shaderCpuUav = mGlobalResources->CbvSrvUavAllocator->GetShaderCpuHandle(mGpuUavAllocInfo.get());
	shaderCpuUav.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderCpuUav;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::Pass::GetShaderGpuUav(int idx)
{
	auto shaderGpuUav = mGlobalResources->CbvSrvUavAllocator->GetShaderGpuHandle(mGpuUavAllocInfo.get());
	shaderGpuUav.Offset(idx * mGlobalResources->CbvSrvUavAllocator->GetDescriptorSize());

	return shaderGpuUav;
}

