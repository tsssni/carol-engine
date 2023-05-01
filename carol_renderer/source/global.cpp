#include <global.h>

namespace Carol
{
	Microsoft::WRL::ComPtr<ID3D12Debug> gDebugLayer;
	Microsoft::WRL::ComPtr<IDXGIFactory> gDxgiFactory;
	Microsoft::WRL::ComPtr<ID3D12Device> gDevice;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> gCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> gGraphicsCommandList;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> gCommandAllocator;
	std::unique_ptr<CommandAllocatorPool> gCommandAllocatorPool;
	Microsoft::WRL::ComPtr<ID3D12Fence> gFence;
	uint64_t gCpuFenceValue;
	uint64_t gGpuFenceValue;

	Microsoft::WRL::ComPtr<IDxcCompiler3> gDXCCompiler;
	Microsoft::WRL::ComPtr<IDxcUtils> gDXCUtils;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> gDXCIncludeHandler;

	Shader gScreenMS;
	Shader gDisplayPS;
	Shader gCullCS;
	Shader gCullAS;
	Shader gOpaqueCullAS;
	Shader gTransparentCullAS;
	Shader gHistHiZCullCS;
	Shader gOpaqueHistHiZCullAS;
	Shader gTransparentHistHiZCullAS;
	Shader gHiZGenerateCS;
	Shader gDepthStaticCullMS;
	Shader gDepthSkinnedCullMS;
	Shader gMeshStaticMS;
	Shader gMeshSkinnedMS;
	Shader gBlinnPhongPS;
	Shader gPBRPS;
	Shader gSkyBoxMS;
	Shader gSkyBoxPS;
	Shader gBlinnPhongOitppllPS;
	Shader gPBROitppllPS;
	Shader gDrawOitppllPS;
	Shader gNormalsStaticMS;
	Shader gNormalsSkinnedMS;
	Shader gNormalsPS;
	Shader gSsaoCS;
	Shader gEpfCS;
	Shader gVelocityStaticMS;
	Shader gVelocitySkinnedMS;
	Shader gVelocityPS;
	Shader gTaaCS;
	Shader gLDRToneMappingCS;

	D3D12_RASTERIZER_DESC gCullDisabledState;
	D3D12_DEPTH_STENCIL_DESC gDepthLessEqualState;
	D3D12_DEPTH_STENCIL_DESC gDepthDisabledState;
	D3D12_BLEND_DESC gAlphaBlendState;

	std::unique_ptr<DescriptorManager> gDescriptorManager;
	std::unique_ptr<HeapManager> gHeapManager;
	std::unique_ptr<TextureManager> gTextureManager;
	std::unique_ptr<Scene> gScene;
}