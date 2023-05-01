#pragma once
#include <utils.h>
#include <dx12.h>
#include <scene.h>
#include <render_pass.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>

namespace Carol
{
	extern Microsoft::WRL::ComPtr<ID3D12Debug> gDebugLayer;
	extern Microsoft::WRL::ComPtr<IDXGIFactory> gDxgiFactory;
	extern Microsoft::WRL::ComPtr<ID3D12Device> gDevice;
	extern Microsoft::WRL::ComPtr<ID3D12CommandQueue> gCommandQueue;
	extern Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> gGraphicsCommandList;
	extern Microsoft::WRL::ComPtr<ID3D12CommandAllocator> gCommandAllocator;
	extern std::unique_ptr<CommandAllocatorPool> gCommandAllocatorPool;
	extern Microsoft::WRL::ComPtr<ID3D12Fence> gFence;
	extern uint64_t gCpuFenceValue;
	extern uint64_t gGpuFenceValue;

	extern Microsoft::WRL::ComPtr<IDxcCompiler3> gDXCCompiler;
	extern Microsoft::WRL::ComPtr<IDxcUtils> gDXCUtils;
	extern Microsoft::WRL::ComPtr<IDxcIncludeHandler> gDXCIncludeHandler;

	extern Shader gScreenMS;
	extern Shader gDisplayPS;
	extern Shader gCullCS;
	extern Shader gCullAS;
	extern Shader gOpaqueCullAS;
	extern Shader gTransparentCullAS;
	extern Shader gHistHiZCullCS;
	extern Shader gOpaqueHistHiZCullAS;
	extern Shader gTransparentHistHiZCullAS;
	extern Shader gHiZGenerateCS;
	extern Shader gDepthStaticCullMS;
	extern Shader gDepthSkinnedCullMS;
	extern Shader gMeshStaticMS;
	extern Shader gMeshSkinnedMS;
	extern Shader gBlinnPhongPS;
	extern Shader gPBRPS;
	extern Shader gSkyBoxMS;
	extern Shader gSkyBoxPS;
	extern Shader gBlinnPhongOitppllPS;
	extern Shader gPBROitppllPS;
	extern Shader gDrawOitppllPS;
	extern Shader gNormalsStaticMS;
	extern Shader gNormalsSkinnedMS;
	extern Shader gNormalsPS;
	extern Shader gSsaoCS;
	extern Shader gEpfCS;
	extern Shader gVelocityStaticMS;
	extern Shader gVelocitySkinnedMS;
	extern Shader gVelocityPS;
	extern Shader gTaaCS;
	extern Shader gLDRToneMappingCS;

	extern D3D12_RASTERIZER_DESC gCullDisabledState;
	extern D3D12_DEPTH_STENCIL_DESC gDepthLessEqualState;
	extern D3D12_DEPTH_STENCIL_DESC gDepthDisabledState;
	extern D3D12_BLEND_DESC gAlphaBlendState;

	extern std::unique_ptr<DescriptorManager> gDescriptorManager;
	extern std::unique_ptr<HeapManager> gHeapManager;
	extern std::unique_ptr<TextureManager> gTextureManager;
	extern std::unique_ptr<Scene> gScene;
}
