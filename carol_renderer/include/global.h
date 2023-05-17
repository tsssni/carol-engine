#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <wrl/client.h>
#include <memory>

namespace Carol
{
	class CommandAllocatorPool;
	class RootSignature;
	class Shader;
	class DescriptorManager;
	class HeapManager;
	class ShaderManager;
	class TextureManager;
	class ModelManager;
	class Renderer;

	extern Microsoft::WRL::ComPtr<ID3D12Debug> gDebugLayer;
	extern Microsoft::WRL::ComPtr<IDXGIFactory> gDxgiFactory;
	extern Microsoft::WRL::ComPtr<ID3D12Device> gDevice;
	extern Microsoft::WRL::ComPtr<ID3D12CommandQueue> gCommandQueue;
	extern Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> gGraphicsCommandList;
	extern Microsoft::WRL::ComPtr<ID3D12CommandAllocator> gCommandAllocator;
	extern std::unique_ptr<CommandAllocatorPool> gCommandAllocatorPool;
	extern Microsoft::WRL::ComPtr<ID3D12CommandSignature> gCommandSignature;
	extern std::unique_ptr<RootSignature> gRootSignature;
	extern Microsoft::WRL::ComPtr<ID3D12Fence> gFence;
	extern uint64_t gCpuFenceValue;
	extern uint64_t gGpuFenceValue;

	extern std::unique_ptr<DescriptorManager> gDescriptorManager;
	extern std::unique_ptr<HeapManager> gHeapManager;
	extern std::unique_ptr<ShaderManager> gShaderManager;
	extern std::unique_ptr<TextureManager> gTextureManager;
	extern std::unique_ptr<ModelManager> gModelManager;

	extern std::unique_ptr<Renderer> gRenderer;
}
