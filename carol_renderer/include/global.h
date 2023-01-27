#pragma once
#include <utils/d3dx12.h>
#include <comdef.h>
#include <wrl/client.h>
#include <vector>
#include <memory>
#include <unordered_map>

namespace Carol
{
	class HeapManager;
	class HeapAllocInfo;
	class RootSignature;
	class Shader;
	class PSO;
	class DescriptorManager;

	class Scene;
	class TextureManager;
	class DisplayPass;
	class FramePass;

	extern Microsoft::WRL::ComPtr<ID3D12Device> gDevice;
	extern Microsoft::WRL::ComPtr<ID3D12CommandQueue> gCommandQueue;
	extern Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> gCommandList;
	extern Microsoft::WRL::ComPtr<ID3D12CommandSignature> gCommandSignature;

	extern std::unique_ptr<RootSignature> gRootSignature;
	extern std::unique_ptr<HeapManager> gHeapManager;
	extern std::unique_ptr<DescriptorManager> gDescriptorManager;

	extern std::unordered_map<std::wstring, std::unique_ptr<Shader>> gShaders;
	extern std::unordered_map<std::wstring, std::unique_ptr<PSO>> gPSOs;

	extern std::unique_ptr<Scene> gScene;
	extern std::unique_ptr<TextureManager> gTextureManager;
	extern std::unique_ptr<DisplayPass> gDisplayPass;
	extern std::unique_ptr<FramePass> gFramePass;

	extern uint32_t gNumFrame;
	extern uint32_t gCurrFrame;
}
