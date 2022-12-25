#pragma once
#include <utils/d3dx12.h>
#include <vector>

namespace Carol
{
	class Sampler
	{

	};

	class StaticSampler : Sampler
	{
	public:
		static std::vector<CD3DX12_STATIC_SAMPLER_DESC>& GetDefaultStaticSamplers();
	};
}
