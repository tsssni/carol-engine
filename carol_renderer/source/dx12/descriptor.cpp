#include <dx12/descriptor.h>
#include <dx12/resource.h>
#include <utils/bitset.h>
#include <utils/buddy.h>
#include <utils/common.h>
#include <global.h>

namespace Carol {
	using std::vector;
	using std::make_unique;	
	using Microsoft::WRL::ComPtr;
}

Carol::DescriptorAllocator::DescriptorAllocator(
	D3D12_DESCRIPTOR_HEAP_TYPE type,
	uint32_t initNumCpuDescriptors,
	uint32_t initNumGpuDescriptors)
	:mType(type),
	mNumCpuDescriptorsPerHeap(initNumCpuDescriptors),
	mNumGpuDescriptorsPerSection(initNumGpuDescriptors),
	mCpuDeletedAllocInfo(gNumFrame),
	mGpuDeletedAllocInfo(gNumFrame)
{
	mDescriptorSize = gDevice->GetDescriptorHandleIncrementSize(type);
	AddCpuDescriptorHeap();

	if (mType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
	{
		ExpandGpuDescriptorHeap();
	}
}

Carol::DescriptorAllocator::~DescriptorAllocator()
{
}

bool Carol::DescriptorAllocator::CpuAllocate(uint32_t numDescriptors, DescriptorAllocInfo* info)
{
	if (numDescriptors == 0)
	{
		return true;
	}

	BuddyAllocInfo buddyInfo;

	for (int i = 0; i < mCpuBuddies.size(); ++i)
	{
		if (mCpuBuddies[i]->Allocate(numDescriptors, buddyInfo))
		{
			info->Allocator = this;
			info->StartOffset = mNumCpuDescriptorsPerHeap * i + buddyInfo.PageId;
			info->NumDescriptors = numDescriptors;

			return true;
		}
	}

	AddCpuDescriptorHeap();

	if (mCpuBuddies.back()->Allocate(numDescriptors, buddyInfo))
	{
		info->Allocator = this;
		info->StartOffset = mNumCpuDescriptorsPerHeap * (mCpuBuddies.size() - 1) + buddyInfo.PageId;
		info->NumDescriptors = numDescriptors;

		return true;
	}

	return false;
}

bool Carol::DescriptorAllocator::CpuDeallocate(DescriptorAllocInfo* info)
{
	mCpuDeletedAllocInfo[mCurrFrame].push_back(*info);
	info->Allocator = nullptr;
	info->NumDescriptors = 0;
	info->StartOffset = 0;

	return true;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DescriptorAllocator::GetCpuHandle(DescriptorAllocInfo* info, uint32_t offset)
{
	uint32_t heapId = info->StartOffset / mNumCpuDescriptorsPerHeap;
	uint32_t startOffset = info->StartOffset % mNumCpuDescriptorsPerHeap;
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mCpuDescriptorHeaps[heapId]->GetCPUDescriptorHandleForHeapStart(), startOffset, mDescriptorSize).Offset(offset * mDescriptorSize);
}

bool Carol::DescriptorAllocator::GpuAllocate(uint32_t numDescriptors, DescriptorAllocInfo* info)
{
	if (numDescriptors == 0)
	{
		return true;
	}

	if (mType != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
	{
		return false;
	}

	BuddyAllocInfo buddyInfo;

	for (int i = 0; i < mGpuBuddies.size(); ++i)
	{
		if (mGpuBuddies[i]->Allocate(numDescriptors, buddyInfo))
		{
			info->Allocator = this;
			info->StartOffset = mNumGpuDescriptorsPerSection * i + buddyInfo.PageId;
			info->NumDescriptors = numDescriptors;

			return true;
		}
	}
	
	ExpandGpuDescriptorHeap();

	if (mGpuBuddies.back()->Allocate(numDescriptors, buddyInfo))
	{
		info->Allocator = this;
		info->StartOffset = mNumGpuDescriptors * (mGpuBuddies.size() - 1) + buddyInfo.PageId;
		info->NumDescriptors = numDescriptors;

		return true;
	}

	return false;
}

bool Carol::DescriptorAllocator::GpuDeallocate(DescriptorAllocInfo* info)
{
	mGpuDeletedAllocInfo[mCurrFrame].push_back(*info);
	info->Allocator = nullptr;
	info->NumDescriptors = 0;
	info->StartOffset = 0;

	return true;
}

void Carol::DescriptorAllocator::DelayedDelete()
{
	for (auto& info : mCpuDeletedAllocInfo[gCurrFrame])
	{
		CpuDelete(&info);
	}

	for (auto& info : mGpuDeletedAllocInfo[gCurrFrame])
	{
		GpuDelete(&info);
	}

	mCpuDeletedAllocInfo[gCurrFrame].clear();
	mGpuDeletedAllocInfo[gCurrFrame].clear();
}

void Carol::DescriptorAllocator::CreateCbv(D3D12_CONSTANT_BUFFER_VIEW_DESC* cbvDesc, DescriptorAllocInfo* info, uint32_t offset)
{
	gDevice->CreateConstantBufferView(cbvDesc, GetCpuHandle(info, offset));
}

void Carol::DescriptorAllocator::CreateSrv(D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc, Resource* resource, DescriptorAllocInfo* info, uint32_t offset)
{
	gDevice->CreateShaderResourceView(resource->Get(), srvDesc, GetCpuHandle(info, offset));
}

void Carol::DescriptorAllocator::CreateUav(D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc, Resource* resource, Resource* counterResource, DescriptorAllocInfo* info, uint32_t offset)
{
	gDevice->CreateUnorderedAccessView(resource->Get(), counterResource ? counterResource->Get() : nullptr, uavDesc, GetCpuHandle(info, offset));
}

void Carol::DescriptorAllocator::CreateRtv(D3D12_RENDER_TARGET_VIEW_DESC* rtvDesc, Resource* resource, DescriptorAllocInfo* info, uint32_t offset)
{
	gDevice->CreateRenderTargetView(resource->Get(), rtvDesc, GetCpuHandle(info, offset));
}

void Carol::DescriptorAllocator::CreateDsv(D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDesc, Resource* resource, DescriptorAllocInfo* info, uint32_t offset)
{
	gDevice->CreateDepthStencilView(resource->Get(), dsvDesc, GetCpuHandle(info, offset));
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DescriptorAllocator::GetShaderCpuHandle(DescriptorAllocInfo* info, uint32_t offset)
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mGpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), info->StartOffset, mDescriptorSize).Offset(offset * mDescriptorSize);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::DescriptorAllocator::GetGpuHandle(DescriptorAllocInfo* info, uint32_t offset)
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(mGpuDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), info->StartOffset, mDescriptorSize).Offset(offset * mDescriptorSize);
}

void Carol::DescriptorAllocator::CopyDescriptors(DescriptorAllocInfo* cpuInfo, DescriptorAllocInfo* shaderCpuInfo)
{
	if (cpuInfo->Allocator == this && shaderCpuInfo->Allocator == this)
	{
		gDevice->CopyDescriptorsSimple(cpuInfo->NumDescriptors, GetShaderCpuHandle(shaderCpuInfo), GetCpuHandle(cpuInfo), mType);
	}
}

uint32_t Carol::DescriptorAllocator::GetDescriptorSize()
{
	return mDescriptorSize;
}

ID3D12DescriptorHeap* Carol::DescriptorAllocator::GetGpuDescriptorHeap()
{
	return mGpuDescriptorHeap.Get();
}

void Carol::DescriptorAllocator::AddCpuDescriptorHeap()
{
	mCpuDescriptorHeaps.emplace_back();
	mCpuBuddies.emplace_back(make_unique<Buddy>(mNumCpuDescriptorsPerHeap, 1u));

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Type = mType;
	descHeapDesc.NumDescriptors = mNumCpuDescriptorsPerHeap;
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	descHeapDesc.NodeMask = 0;

	ThrowIfFailed(gDevice->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(mCpuDescriptorHeaps.back().GetAddressOf())));
}

void Carol::DescriptorAllocator::ExpandGpuDescriptorHeap()
{
	mNumGpuDescriptors = (mNumGpuDescriptors == 0) ? mNumGpuDescriptorsPerSection : mNumGpuDescriptors * 2;
	mGpuBuddies.emplace_back(make_unique<Buddy>(mNumGpuDescriptorsPerSection, 1u));

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Type = mType;
	descHeapDesc.NumDescriptors = mNumGpuDescriptors;
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	
	ComPtr<ID3D12DescriptorHeap> oldHeap = mGpuDescriptorHeap;
	ThrowIfFailed(gDevice->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(mGpuDescriptorHeap.GetAddressOf())));

	if (oldHeap)
	{
		gDevice->CopyDescriptorsSimple(mNumGpuDescriptors >> 1, mGpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), oldHeap->GetCPUDescriptorHandleForHeapStart(), mType);
	}
}

bool Carol::DescriptorAllocator::CpuDelete(DescriptorAllocInfo* info)
{
	if (info->NumDescriptors == 0) 
	{
		return true;
	}

	uint32_t buddyIdx = info->StartOffset / mNumCpuDescriptorsPerHeap;
	uint32_t blockIdx = info->StartOffset % mNumCpuDescriptorsPerHeap;
	BuddyAllocInfo buddyInfo(blockIdx, info->NumDescriptors);

	if (mCpuBuddies[buddyIdx]->Deallocate(buddyInfo))
	{
		info->Allocator = nullptr;
		info->NumDescriptors = 0;
		info->StartOffset = 0;

		return true;
	}
	
	return false;
}

bool Carol::DescriptorAllocator::GpuDelete(DescriptorAllocInfo* info)
{
	if (info->NumDescriptors == 0)
	{
		return true;
	}

	uint32_t buddyIdx = info->StartOffset / mNumGpuDescriptorsPerSection;
	uint32_t blockIdx = info->StartOffset % mNumGpuDescriptorsPerSection;
	BuddyAllocInfo buddyInfo(blockIdx, info->NumDescriptors);

	if (mGpuBuddies[buddyIdx]->Deallocate(buddyInfo))
	{
		info->Allocator = nullptr;
		info->NumDescriptors = 0;
		info->StartOffset = 0;

		return true;
	}
	
	return false;
}

Carol::DescriptorManager::DescriptorManager(uint32_t initCpuCbvSrvUavHeapSize, uint32_t initGpuCbvSrvUavHeapSize, uint32_t initRtvHeapSize, uint32_t initDsvHeapSize)
{
	mCbvSrvUavAllocator = make_unique<DescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, initCpuCbvSrvUavHeapSize, initGpuCbvSrvUavHeapSize);
	mRtvAllocator = make_unique<DescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, initRtvHeapSize, 0);
	mDsvAllocator = make_unique<DescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, initDsvHeapSize, 0);
}

void Carol::DescriptorManager::DelayedDelete()
{
	mCbvSrvUavAllocator->DelayedDelete();
	mRtvAllocator->DelayedDelete();
	mDsvAllocator->DelayedDelete();
}

Carol::DescriptorManager::DescriptorManager(Carol::DescriptorManager&& manager)
{
	mCbvSrvUavAllocator = std::move(manager.mCbvSrvUavAllocator);
	mRtvAllocator = std::move(manager.mRtvAllocator);
	mDsvAllocator = std::move(manager.mDsvAllocator);
}

void Carol::DescriptorManager::CpuCbvSrvUavAllocate(uint32_t numDescriptors, DescriptorAllocInfo* info)
{
	mCbvSrvUavAllocator->CpuAllocate(numDescriptors, info);
}

void Carol::DescriptorManager::GpuCbvSrvUavAllocate(uint32_t numDescriptors, DescriptorAllocInfo* info)
{
	mCbvSrvUavAllocator->GpuAllocate(numDescriptors, info);
}

void Carol::DescriptorManager::RtvAllocate(uint32_t numDescriptors, DescriptorAllocInfo* info)
{
	mRtvAllocator->CpuAllocate(numDescriptors, info);
}

void Carol::DescriptorManager::DsvAllocate(uint32_t numDescriptors, DescriptorAllocInfo* info)
{
	mDsvAllocator->CpuAllocate(numDescriptors, info);
}

void Carol::DescriptorManager::CpuCbvSrvUavDeallocate(DescriptorAllocInfo* info)
{
	mCbvSrvUavAllocator->CpuDeallocate(info);
}

void Carol::DescriptorManager::GpuCbvSrvUavDeallocate(DescriptorAllocInfo* info)
{
	mCbvSrvUavAllocator->GpuDeallocate(info);
}
void Carol::DescriptorManager::RtvDeallocate(DescriptorAllocInfo* info)
{
	mRtvAllocator->CpuDeallocate(info);
}

void Carol::DescriptorManager::DsvDeallocate(DescriptorAllocInfo* info)
{
	mDsvAllocator->CpuDeallocate(info);
}

void Carol::DescriptorManager::CreateCbv(D3D12_CONSTANT_BUFFER_VIEW_DESC* cbvDesc, DescriptorAllocInfo* info, uint32_t offset)
{
	mCbvSrvUavAllocator->CreateCbv(cbvDesc, info, offset);
}

void Carol::DescriptorManager::CreateSrv(D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc, Resource* resource, DescriptorAllocInfo* info, uint32_t offset)
{
	mCbvSrvUavAllocator->CreateSrv(srvDesc, resource, info, offset);
}

void Carol::DescriptorManager::CreateUav(D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc, Resource* resource, Resource* counterResource, DescriptorAllocInfo* info, uint32_t offset)
{
	mCbvSrvUavAllocator->CreateUav(uavDesc, resource, counterResource, info, offset);
}
void Carol::DescriptorManager::CreateRtv(D3D12_RENDER_TARGET_VIEW_DESC* rtvDesc, Resource* resource, DescriptorAllocInfo* info, uint32_t offset)
{
	mRtvAllocator->CreateRtv(rtvDesc, resource, info, offset);
}

void Carol::DescriptorManager::CreateDsv(D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDesc, Resource* resource, DescriptorAllocInfo* info, uint32_t offset)
{
	mDsvAllocator->CreateDsv(dsvDesc, resource, info, offset);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DescriptorManager::GetCpuCbvSrvUavHandle(DescriptorAllocInfo* info, uint32_t offset)
{
	return mCbvSrvUavAllocator->GetCpuHandle(info, offset);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DescriptorManager::GetShaderCpuCbvSrvUavHandle(DescriptorAllocInfo* info, uint32_t offset)
{
	return mCbvSrvUavAllocator->GetShaderCpuHandle(info, offset);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Carol::DescriptorManager::GetGpuCbvSrvUavHandle(DescriptorAllocInfo* info, uint32_t offset)
{
	return mCbvSrvUavAllocator->GetGpuHandle(info, offset);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DescriptorManager::GetRtvHandle(DescriptorAllocInfo* info, uint32_t offset)
{
	return mRtvAllocator->GetCpuHandle(info, offset);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Carol::DescriptorManager::GetDsvHandle(DescriptorAllocInfo* info, uint32_t offset)
{
	return mDsvAllocator->GetCpuHandle(info, offset);
}

uint32_t Carol::DescriptorManager::GetCbvSrvUavSize()
{
	return mCbvSrvUavAllocator->GetDescriptorSize();
}

uint32_t Carol::DescriptorManager::GetRtvSize()
{
	return mRtvAllocator->GetDescriptorSize();
}

uint32_t Carol::DescriptorManager::GetDsvSize()
{
	return mDsvAllocator->GetDescriptorSize();
}

ID3D12DescriptorHeap* Carol::DescriptorManager::GetResourceDescriptorHeap()
{
	return mCbvSrvUavAllocator->GetGpuDescriptorHeap();
}

void Carol::DescriptorManager::CopyCbvSrvUav(DescriptorAllocInfo* cpuInfo, DescriptorAllocInfo* shaderCpuInfo)
{
	mCbvSrvUavAllocator->CopyDescriptors(cpuInfo, shaderCpuInfo);
}
