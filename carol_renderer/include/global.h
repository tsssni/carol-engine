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
	class TextureManager;
	class Scene;
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

	extern Microsoft::WRL::ComPtr<IDxcCompiler3> gDXCCompiler;
	extern Microsoft::WRL::ComPtr<IDxcUtils> gDXCUtils;
	extern Microsoft::WRL::ComPtr<IDxcIncludeHandler> gDXCIncludeHandler;

	extern std::unique_ptr<Shader> gScreenMS;
	extern std::unique_ptr<Shader> gDisplayPS;
	extern std::unique_ptr<Shader> gCullCS;
	extern std::unique_ptr<Shader> gCullAS;
	extern std::unique_ptr<Shader> gOpaqueCullAS;
	extern std::unique_ptr<Shader> gTransparentCullAS;
	extern std::unique_ptr<Shader> gHistHiZCullCS;
	extern std::unique_ptr<Shader> gOpaqueHistHiZCullAS;
	extern std::unique_ptr<Shader> gTransparentHistHiZCullAS;
	extern std::unique_ptr<Shader> gHiZGenerateCS;
	extern std::unique_ptr<Shader> gDepthStaticCullMS;
	extern std::unique_ptr<Shader> gDepthSkinnedCullMS;
	extern std::unique_ptr<Shader> gMeshStaticMS;
	extern std::unique_ptr<Shader> gMeshSkinnedMS;
	extern std::unique_ptr<Shader> gGeometryPS;
	extern std::unique_ptr<Shader> gShadeCS;
	extern std::unique_ptr<Shader> gSkyBoxMS;
	extern std::unique_ptr<Shader> gSkyBoxPS;
	extern std::unique_ptr<Shader> gOitStaticMS;
	extern std::unique_ptr<Shader> gOitSkinnedMS;
	extern std::unique_ptr<Shader> gBuildOitppllPS;
	extern std::unique_ptr<Shader> gOitppllCS;
	extern std::unique_ptr<Shader> gSsaoCS;
	extern std::unique_ptr<Shader> gEpfCS;
	extern std::unique_ptr<Shader> gTaaCS;
	extern std::unique_ptr<Shader> gLDRToneMappingCS;

	extern std::unique_ptr<D3D12_RASTERIZER_DESC> gCullDisabledState;
	extern std::unique_ptr<D3D12_DEPTH_STENCIL_DESC> gDepthLessEqualState;
	extern std::unique_ptr<D3D12_DEPTH_STENCIL_DESC> gDepthDisabledState;
	extern std::unique_ptr<D3D12_BLEND_DESC> gAlphaBlendState;

	extern std::unique_ptr<DescriptorManager> gDescriptorManager;
	extern std::unique_ptr<HeapManager> gHeapManager;
	extern std::unique_ptr<TextureManager> gTextureManager;
	extern std::unique_ptr<Scene> gScene;

	extern std::unique_ptr<Renderer> gRenderer;
}
