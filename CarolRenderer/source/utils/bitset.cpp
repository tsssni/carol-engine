#include <utils/bitset.h>

Carol::Bitset::Bitset(uint32_t numPages)
{
	mBitset.resize(std::max((numPages + 63) / 64, 1u), 0);
	mNumPages = numPages;
}

bool Carol::Bitset::Set(uint32_t idx)
{
	if (idx >= mNumPages || ((mBitset[idx / 64] >> ((uint64_t)idx % 64ull)) & 1ull) == 1ull)
	{
		return false;
	}

	mBitset[idx/64] = mBitset[idx / 64] ^ (1ull << ((uint64_t)idx % 64ull));
	return true;
}

bool Carol::Bitset::Reset(uint32_t idx)
{
	if (idx >= mNumPages || ((mBitset[idx / 64] >> ((uint64_t)idx % 64ull)) & 1ull) == 0ull)
	{
		return false;
	}

	mBitset[idx/64] = mBitset[idx / 64] ^ (1ull << ((uint64_t)idx % 64ull));
	return true;
}

bool Carol::Bitset::IsPageIdle(uint32_t idx)
{
	return !(mBitset[idx / 64] & (1ull << ((uint64_t)idx % 64ull)));
}