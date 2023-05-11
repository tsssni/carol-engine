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

	Microsoft::WRL::ComPtr<IDxcCompiler3> gDXCCompiler;
	Microsoft::WRL::ComPtr<IDxcUtils> gDXCUtils;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> gDXCIncludeHandler;

	std::unique_ptr<Shader> gScreenMS;
	std::unique_ptr<Shader> gDisplayPS;
	std::unique_ptr<Shader> gCullCS;
	std::unique_ptr<Shader> gCullAS;
	std::unique_ptr<Shader> gOpaqueCullAS;
	std::unique_ptr<Shader> gTransparentCullAS;
	std::unique_ptr<Shader> gHistHiZCullCS;
	std::unique_ptr<Shader> gOpaqueHistHiZCullAS;
	std::unique_ptr<Shader> gTransparentHistHiZCullAS;
	std::unique_ptr<Shader> gHiZGenerateCS;
	std::unique_ptr<Shader> gDepthStaticCullMS;
	std::unique_ptr<Shader> gDepthSkinnedCullMS;
	std::unique_ptr<Shader> gMeshStaticMS;
	std::unique_ptr<Shader> gMeshSkinnedMS;
	std::unique_ptr<Shader> gGeometryPS;
	std::unique_ptr<Shader> gShadeCS;
	std::unique_ptr<Shader> gOitStaticMS;
	std::unique_ptr<Shader> gOitSkinnedMS;
	std::unique_ptr<Shader> gBuildOitppllPS;
	std::unique_ptr<Shader> gOitppllCS;
	std::unique_ptr<Shader> gSsaoCS;
	std::unique_ptr<Shader> gEpfCS;
	std::unique_ptr<Shader> gTaaCS;
	std::unique_ptr<Shader> gLDRToneMappingCS;

	std::unique_ptr<D3D12_RASTERIZER_DESC> gCullDisabledState;
	std::unique_ptr<D3D12_DEPTH_STENCIL_DESC> gDepthLessEqualState;
	std::unique_ptr<D3D12_DEPTH_STENCIL_DESC> gDepthDisabledState;
	std::unique_ptr<D3D12_BLEND_DESC> gAlphaBlendState;

	std::unique_ptr<DescriptorManager> gDescriptorManager;
	std::unique_ptr<HeapManager> gHeapManager;
	std::unique_ptr<TextureManager> gTextureManager;
	std::unique_ptr<Scene> gScene;

	std::unique_ptr<Renderer> gRenderer;
}