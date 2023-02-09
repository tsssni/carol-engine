#pragma once
#include <global.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <queue>
#include <vector>
#include <mutex>

namespace Carol
{
	class CommandAllocatorPool
	{
	public:
		CommandAllocatorPool(D3D12_COMMAND_LIST_TYPE type, ID3D12Device* device);
		CommandAllocatorPool(CommandAllocatorPool&& commandAllocatorPool);
		CommandAllocatorPool& operator=(CommandAllocatorPool&& commandAllocatorPool);

		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> RequestAllocator(uint64_t completedFenceValue);
		void DiscardAllocator(ID3D12CommandAllocator* allocator, uint64_t cpuFenceValue);
	protected:
		std::queue<std::pair<uint64_t, Microsoft::WRL::ComPtr<ID3D12CommandAllocator>>> mAllocatorQueue;
		D3D12_COMMAND_LIST_TYPE mType;
		ID3D12Device* mDevice;
		std::mutex mAllocatorMutex;
	};
}
