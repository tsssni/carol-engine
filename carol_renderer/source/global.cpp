#include <carol.h>

namespace Carol
{
	Microsoft::WRL::ComPtr<ID3D12Debug> gDebugLayer;
	Microsoft::WRL::ComPtr<IDXGIFactory> gDxgiFactory;
	Microsoft::WRL::ComPtr<ID3D12Device> gDevice;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> gCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> gGraphicsCommandList;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> gCommandAllocator;
	std::unique_ptr<CommandAllocatorPool> gCommandAllocatorPool;
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> gCommandSignature;
	std::unique_ptr<RootSignature> gRootSignature;
	Microsoft::WRL::ComPtr<ID3D12Fence> gFence;
	uint64_t gCpuFenceValue;
	uint64_t gGpuFenceValue;

	std::unique_ptr<DescriptorManager> gDescriptorManager;
	std::unique_ptr<HeapManager> gHeapManager;
	std::unique_ptr<ShaderManager> gShaderManager;
	std::unique_ptr<TextureManager> gTextureManager;
	std::unique_ptr<ModelManager> gModelManager;

	std::unique_ptr<Renderer> gRenderer;
}