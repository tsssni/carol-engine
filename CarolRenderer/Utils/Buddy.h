#pragma once
#include "Bitset.h"
#include <vector>
#include <list>
#include <memory>

using std::vector;
using std::list;
using std::unique_ptr;

namespace Carol
{
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
		bool Deallocate(BuddyAllocInfo info);
	private:
		uint32_t GetOrder(uint32_t size);
		bool CheckIsAllocated(BuddyAllocInfo info);
		bool BuddyMerge(BuddyAllocInfo info);

		void SetAllocated(BuddyAllocInfo info);
		void SetDeallocated(BuddyAllocInfo info);

		vector<unique_ptr<Bitset>> mBitsets;
		vector<list<BuddyAllocInfo>> mFreeAreas;

		uint32_t mPageSize;
		uint32_t mOrder;
	};
}
