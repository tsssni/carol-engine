export module carol.renderer.utils.buddy;
import carol.renderer.utils.bitset;
import <vector>;
import <list>;
import <memory>;
import <cmath>;

namespace Carol
{
    using std::list;
    using std::make_unique;
    using std::unique_ptr;
    using std::vector;

    export class BuddyAllocInfo
	{
	public:
		uint32_t PageId = 0;
		uint32_t NumPages = 0;
	};

	export class Buddy
	{
	public:
		Buddy(uint32_t size, uint32_t pageSize)
            :mPageSize(pageSize), mOrder(GetOrder(size))
        {
            mFreeAreas.resize(mOrder + 1);
            mBitsets.resize(mOrder + 1);
            mFreeAreas[mOrder].push_back({ 0,1u << mOrder });

            for (int i = 0; i <= mOrder; ++i)
            {
                mBitsets[i] = make_unique<Bitset>(1 << (mOrder - i));
            }
        }

		bool Allocate(uint32_t size, BuddyAllocInfo& info)
        {
            if (size <= 0 || size > (1 << mOrder) * mPageSize)
            {
                return false;
            }

            uint32_t order = GetOrder(size);
            
            if (!mFreeAreas[order].empty())
            {
                info = mFreeAreas[order].front();
                mFreeAreas[order].pop_front();

                SetAllocated(info);
                return true;
            }
            else
            {
                uint32_t allocatedPages = 1 << order;

                while (order <= mOrder && mFreeAreas[order].empty())
                {
                    ++order;
                }
                
                if (order > mOrder)
                {
                    return false;
                }

                auto freeArea = mFreeAreas[order].front();
                mFreeAreas[order].pop_front();

                while (freeArea.NumPages > allocatedPages)
                {
                    mFreeAreas[--order].push_back({ freeArea.PageId + (freeArea.NumPages >> 1),freeArea.NumPages >> 1 });
                    freeArea.NumPages >>= 1;
                }

                info = freeArea;
                SetAllocated(info);
                return true;
            }
        }

		void Deallocate(BuddyAllocInfo& info)
        {
            if (info.NumPages > 0)
            {
                info.NumPages = 1 << GetOrder(info.NumPages);

                if (CheckIsAllocated(info))
                {
                    if (BuddyMerge(info))
                    {
                        mFreeAreas[GetOrder(info.NumPages)].push_back(info);
                    }

                    SetDeallocated(info);
                }
            }
        }

	private:
		uint32_t GetOrder(uint32_t size)
        {
            size = std::ceil(size * 1.0f / mPageSize);
            return std::ceil(std::log2(size));
        }

		bool CheckIsAllocated(BuddyAllocInfo info)
        {
            for (int i = 0; i <= log2(info.NumPages); ++i)
            {
                uint32_t orderPageId = info.PageId / (1 << i);
                uint32_t orderNumAreas = info.NumPages / (1 << i);
                
                for (int j = 0; j < orderNumAreas; ++j)
                {
                    if (mBitsets[i]->IsPageIdle(orderPageId + j))
                    {
                        return false;
                    }
                }
            }

            return true;
        }

		bool BuddyMerge(BuddyAllocInfo info)
        {
            uint32_t order = std::log2(info.NumPages);
            uint32_t orderPageId = info.PageId / (1 << order);

            if (order == mOrder)
            {
                return true;
            }

            static int buddySign[2] = { 1,-1 };
            uint32_t buddyOrderPageId = orderPageId + buddySign[orderPageId % 2];
            
            if (mBitsets[order]->IsPageIdle(buddyOrderPageId))
            {
                uint32_t buddyPageId = buddyOrderPageId * (1 << order);

                for (auto freeAreaItr = mFreeAreas[order].begin(); freeAreaItr != mFreeAreas[order].end(); ++freeAreaItr)
                {
                    if (freeAreaItr->PageId == buddyPageId)
                    {
                        BuddyAllocInfo mergeInfo;
                        mergeInfo.PageId = std::min(info.PageId, buddyPageId);
                        mergeInfo.NumPages = info.NumPages << 1;
                        mFreeAreas[order].erase(freeAreaItr);
                        
                        if (BuddyMerge(mergeInfo))
                        {
                            mFreeAreas[order + 1].push_back(mergeInfo);
                        }

                        return false;
                    }
                }
            }

            return true;
        }

		void SetAllocated(BuddyAllocInfo info)
        {
            for (int i = 0; i <= mOrder; ++i)
            {
                uint32_t orderPageId = info.PageId / (1 << i);
                uint32_t orderNumAreas = info.NumPages / (1 << i);
                if (orderNumAreas == 0)
                {
                    orderNumAreas = 1;
                }

                for (int j = 0; j < orderNumAreas; ++j)
                {
                    if (!mBitsets[i]->Set(orderPageId + j))
                    {
                        return;
                    }
                }
            }
        }

		void SetDeallocated(BuddyAllocInfo info)
        {
            for (int i = 0; i <= mOrder; ++i)
            {
                uint32_t orderPageId = info.PageId / (1 << i);
                uint32_t orderNumAreas = info.NumPages / (1 << i);
                
                if (orderNumAreas)
                {
                    for (int j = 0; j < orderNumAreas; ++j)
                    {
                        mBitsets[i]->Reset(orderPageId + j);
                    }
                }
                else
                {
                    uint32_t lastOrderPageId = orderPageId << 1;
                    
                    if (mBitsets[i - 1]->IsPageIdle(lastOrderPageId) && mBitsets[i - 1]->IsPageIdle(lastOrderPageId + 1))
                    {
                        mBitsets[i]->Reset(orderPageId);
                    }
                }
            }
        }

		vector<unique_ptr<Bitset>> mBitsets;
		vector<list<BuddyAllocInfo>> mFreeAreas;

		uint32_t mPageSize;
		uint32_t mOrder;
	};
}