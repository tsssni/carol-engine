#pragma once
#include <vector>
#include <list>
#include <memory>

namespace Carol
{
	class Bitset;

	class BuddyAllocInfo
	{
	public:
		uint32_t PageId = 0;
		uint32_t NumPages = 0;
	};

	class Buddy
	{
	public:
		Buddy(uint32_t size, uint32_t pageSize);
		bool Allocate(uint32_t size, BuddyAllocInfo& info);
		void Deallocate(BuddyAllocInfo& info);
	private:
		uint32_t GetOrder(uint32_t size);
		bool CheckIsAllocated(BuddyAllocInfo info);
		bool BuddyMerge(BuddyAllocInfo info);

		void SetAllocated(BuddyAllocInfo info);
		void SetDeallocated(BuddyAllocInfo info);

		std::vector<std::unique_ptr<Bitset>> mBitsets;
		std::vector<std::list<BuddyAllocInfo>> mFreeAreas;

		uint32_t mPageSize;
		uint32_t mOrder;
	};
}
