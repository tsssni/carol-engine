#include <dx12/command.h>
#include <utils/exception.h>
#include <global.h>

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

Microsoft::WRL::ComPtr<ID3D12CommandAllocator> Carol::CommandAllocatorPool::RequestAllocator(uint64_t completedFenceValue)
{
    std::lock_guard<std::mutex> lock(mAllocatorMutex);

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator = nullptr;

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
		mAllocatorQueue.emplace(std::make_pair(cpuFenceValue, Microsoft::WRL::ComPtr<ID3D12CommandAllocator>(allocator)));
	}
}
