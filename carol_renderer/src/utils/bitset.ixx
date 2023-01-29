export module carol.renderer.utils.bitset;
import <vector>;

namespace Carol
{
    using std::vector;

    export class Bitset
    {
    public:
		Bitset(uint32_t numPages)
        {
            mBitset.resize(std::max((numPages + 63) / 64, 1u), 0);
            mNumPages = numPages;
        }

		bool Set(uint32_t idx)
        {
            if (idx >= mNumPages || ((mBitset[idx / 64] >> ((uint64_t)idx % 64ull)) & 1ull) == 1ull)
            {
                return false;
            }

            mBitset[idx/64] = mBitset[idx / 64] ^ (1ull << ((uint64_t)idx % 64ull));
            return true;
        }

		bool Reset(uint32_t idx)
        {
            if (idx >= mNumPages || ((mBitset[idx / 64] >> ((uint64_t)idx % 64ull)) & 1ull) == 0ull)
            {
                return false;
            }

            mBitset[idx/64] = mBitset[idx / 64] ^ (1ull << ((uint64_t)idx % 64ull));
            return true;
        }

		bool IsPageIdle(uint32_t idx)
        {
            return !(mBitset[idx / 64] & (1ull << ((uint64_t)idx % 64ull)));
        }

	private:
		vector<uint64_t> mBitset;
		uint32_t mNumPages;
    };
}