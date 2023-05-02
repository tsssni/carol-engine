#include <dx12/command.h>
#include <utils/common.h>
#include <global.h>

namespace Carol
{
    using std::make_pair;
	using std::lock_guard;
	using std::mutex;
    using Microsoft::WRL::ComPtr;
}

Carol::CommandAllocatorPool::CommandAllocatorPool(D3D12_COMMAND_LIST_TYPE type)
    :mType(type)
{
}

Carol::CommandAllocatorPool::CommandAllocatorPool(CommandAllocatorPool&& commandAllocatorPool)
    :mAllocatorQueue(std::move(commandAllocatorPool.mAllocatorQueue)),
    mType(commandAllocatorPool.mType)
{
}

Carol::CommandAllocatorPool& Carol::CommandAllocatorPool::operator=(CommandAllocatorPool&& commandAllocatorPool)
{
    this->CommandAllocatorPool::CommandAllocatorPool(std::move(commandAllocatorPool));
    return *this;
}

Carol::ComPtr<ID3D12CommandAllocator> Carol::CommandAllocatorPool::RequestAllocator(uint64_t completedFenceValue)
{
    lock_guard<mutex> lock(mAllocatorMutex);

	ComPtr<ID3D12CommandAllocator> allocator = nullptr;

	if (!mAllocatorQueue.empty() && mAllocatorQueue.front().first <= completedFenceValue)
	{
		allocator = std::move(mAllocatorQueue.front().second);
		ThrowIfFailed(allocator->Reset());
		mAllocatorQueue.pop();
	}

	if (!allocator)
	{
		ThrowIfFailed(gDevice->CreateCommandAllocator(mType, IID_PPV_ARGS(allocator.GetAddressOf())));
	}
	
	return allocator;
}

void Carol::CommandAllocatorPool::DiscardAllocator(ID3D12CommandAllocator* allocator, uint64_t cpuFenceValue)
{
	if (allocator)
	{
		mAllocatorQueue.emplace(make_pair(cpuFenceValue, ComPtr<ID3D12CommandAllocator>(allocator)));
	}
}
