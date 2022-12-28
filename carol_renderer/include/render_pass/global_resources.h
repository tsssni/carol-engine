#pragma once
#include <d3d12.h>
#include <comdef.h>
#include <wrl/client.h>
#include <vector>
#include <memory>
#include <unordered_map>

namespace Carol
{
	class Heap;
	class HeapAllocInfo;
	class RootSignature;
	class Shader;
	class CircularHeap;
	class DescriptorAllocator;
	class CbvSrvUavDescriptorAllocator;
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
		ID3D12Device* Device;
		ID3D12CommandQueue* CommandQueue;
		ID3D12GraphicsCommandList* CommandList;

		Heap* DefaultBuffersHeap;
		Heap* UploadBuffersHeap;
		Heap* ReadbackBuffersHeap;
		Heap* TexturesHeap;

		RootSignature* RootSignature;
		CbvSrvUavDescriptorAllocator* CbvSrvUavAllocator;
		DescriptorAllocator* RtvAllocator;
		DescriptorAllocator* DsvAllocator;

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
		D3D12_GRAPHICS_PIPELINE_STATE_DESC* BasePsoDesc;
	};
}
