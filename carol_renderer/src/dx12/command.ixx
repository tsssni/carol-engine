export module carol.renderer.dx12.command;
import carol.renderer.utils;
import <wrl/client.h>;
import <comdef.h>;
import <queue>;
import <vector>;
import <mutex>;

namespace Carol
{
    using Microsoft::WRL::ComPtr;
    using std::lock_guard;
    using std::make_pair;
    using std::mutex;
    using std::pair;
    using std::queue;

    export class CommandAllocatorPool
	{
	public:
		CommandAllocatorPool(D3D12_COMMAND_LIST_TYPE type, ID3D12Device* device)
            :mType(type), mDevice(device)
        {
        }

		CommandAllocatorPool(CommandAllocatorPool&& commandAllocatorPool)
            :mAllocatorQueue(std::move(commandAllocatorPool.mAllocatorQueue)),
            mType(commandAllocatorPool.mType),
            mDevice(commandAllocatorPool.mDevice)
        {
        }

		CommandAllocatorPool& operator=(CommandAllocatorPool&& commandAllocatorPool)
        {
            this->CommandAllocatorPool::CommandAllocatorPool(std::move(commandAllocatorPool));
            return *this;
        }

		ComPtr<ID3D12CommandAllocator> RequestAllocator(uint64_t completedFenceValue)
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
                ThrowIfFailed(mDevice->CreateCommandAllocator(mType, IID_PPV_ARGS(allocator.GetAddressOf())));
            }
            
            return allocator;
        }

		void DiscardAllocator(ID3D12CommandAllocator* allocator, uint64_t cpuFenceValue)
        {
            if (allocator)
            {
                mAllocatorQueue.emplace(make_pair(cpuFenceValue, ComPtr<ID3D12CommandAllocator>(allocator)));
            }
        }

	protected:
		queue<pair<uint64_t, ComPtr<ID3D12CommandAllocator>>> mAllocatorQueue;
		D3D12_COMMAND_LIST_TYPE mType;
		ID3D12Device* mDevice;
		mutex mAllocatorMutex;
	};
}