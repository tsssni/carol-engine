#pragma once
#include <utils/d3dx12.h>
#include <comdef.h>
#include <wrl/client.h>
#include <vector>
#include <memory>
#include <unordered_map>

namespace Carol
{
	class Shader;
	class PSO;

	extern std::unordered_map<std::wstring, std::unique_ptr<Shader>> gShaders;
	extern std::unordered_map<std::wstring, std::unique_ptr<PSO>> gPSOs;

	extern uint32_t gNumFrame;
	extern uint32_t gCurrFrame;
}
