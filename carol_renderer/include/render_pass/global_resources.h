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
	class DescriptorManager;
	class Display;
	class FramePass;
	class ShadowPass;
	class SsaoPass;
	class NormalPass;
	class TaaPass;
	class OitppllPass;
	class MeshesPass;
	class BoundingRect;
	class Scene;
	class Camera;
	class Timer;
	class AssimpModel;
	class MeshesPass;

	class GlobalResources
	{
	public:
		ID3D12Device2* Device;
		ID3D12CommandQueue* CommandQueue;
		ID3D12GraphicsCommandList6* CommandList;

		RootSignature* RootSignature;
		HeapManager* HeapManager;
		DescriptorManager* DescriptorManager;

		Display* Display;
		FramePass* Frame;
		ShadowPass* MainLight;
		NormalPass* Normal;
		SsaoPass* Ssao;
		TaaPass* Taa;
		OitppllPass* Oitppll;
		MeshesPass* Meshes;

		D3D12_VIEWPORT* ScreenViewport;
		D3D12_RECT* ScissorRect;
		
		Scene* Scene;
		Camera* Camera;
		Timer* Timer;
		uint32_t NumFrame;
		uint32_t* CurrFrame;

		uint32_t* ClientWidth;
		uint32_t* ClientHeight;

		std::unordered_map<std::wstring, std::unique_ptr<Shader>>* Shaders;
		std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D12PipelineState>>* PSOs;
		D3DX12_MESH_SHADER_PIPELINE_STATE_DESC* BaseGraphicsPsoDesc;
		D3D12_COMPUTE_PIPELINE_STATE_DESC* BaseComputePsoDesc;
	};
}
