#pragma once
#include <utils/bitset.h>
#include <utils/buddy.h>
#include <utils/common.h>
#include <utils/d3dx12.h>

#include <dx12/command.h>
#include <dx12/descriptor.h>
#include <dx12/heap.h>
#include <dx12/indirect_command.h>
#include <dx12/pipeline_state.h>
#include <dx12/resource.h>
#include <dx12/root_signature.h>
#include <dx12/sampler.h>
#include <dx12/shader.h>

#include <scene/assimp.h>
#include <scene/camera.h>
#include <scene/light.h>
#include <scene/model.h>
#include <scene/scene.h>
#include <scene/scene_node.h>
#include <scene/skinned_data.h>
#include <scene/texture.h>
#include <scene/timer.h>

#include <render_pass/cull_pass.h>
#include <render_pass/display_pass.h>
#include <render_pass/epf_pass.h>
#include <render_pass/frame_pass.h>
#include <render_pass/normal_pass.h>
#include <render_pass/shadow_pass.h>
#include <render_pass/ssao_pass.h>
#include <render_pass/taa_pass.h>
#include <render_pass/tone_mapping_pass.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <wrl/client.h>
#include <memory>

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
	extern std::unique_ptr<Shader> gBlinnPhongPS;
	extern std::unique_ptr<Shader> gPBRPS;
	extern std::unique_ptr<Shader> gSkyBoxMS;
	extern std::unique_ptr<Shader> gSkyBoxPS;
	extern std::unique_ptr<Shader> gBlinnPhongOitppllPS;
	extern std::unique_ptr<Shader> gPBROitppllPS;
	extern std::unique_ptr<Shader> gDrawOitppllPS;
	extern std::unique_ptr<Shader> gNormalsStaticMS;
	extern std::unique_ptr<Shader> gNormalsSkinnedMS;
	extern std::unique_ptr<Shader> gNormalsPS;
	extern std::unique_ptr<Shader> gSsaoCS;
	extern std::unique_ptr<Shader> gEpfCS;
	extern std::unique_ptr<Shader> gVelocityStaticMS;
	extern std::unique_ptr<Shader> gVelocitySkinnedMS;
	extern std::unique_ptr<Shader> gVelocityPS;
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
}
