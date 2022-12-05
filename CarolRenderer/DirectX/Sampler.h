#pragma once
#include "../Utils/d3dx12.h"
#include <vector>

using std::vector;

namespace Carol
{
	class Sampler
	{

	};

	class StaticSampler : Sampler
	{
	public:
		static vector<CD3DX12_STATIC_SAMPLER_DESC>& GetDefaultStaticSamplers();
	};
}
