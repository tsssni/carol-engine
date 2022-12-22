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
	class Display;
	class ShadowPass;
	class SsaoPass;
	class TaaPass;
	class OitppllPass;
	class BoundingRect;
	class Camera;
	class Timer;
	class AssimpModel;
	class MeshPass;
	class FrameConstants;

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

		HeapAllocInfo* PassCBAllocInfo;
		CircularHeap* PassCBHeap;

		RootSignature* RootSignature;
		DescriptorAllocator* CbvSrvUavAllocator;
		DescriptorAllocator* RtvAllocator;
		DescriptorAllocator* DsvAllocator;

		Display* Display;
		ShadowPass* MainLight;
		SsaoPass* Ssao;
		TaaPass* Taa;
		OitppllPass* Oitppll;

		D3D12_VIEWPORT* ScreenViewport;
		D3D12_RECT* ScissorRect;

		Camera* Camera;
		Timer* Timer;

		uint32_t* ClientWidth;
		uint32_t* ClientHeight;

		std::vector<MeshPass*>* OpaqueStaticMeshes;
		std::vector<MeshPass*>* OpaqueSkinnedMeshes;
		std::vector<MeshPass*>* TransparentStaticMeshes;
		std::vector<MeshPass*>* TransparentSkinnedMeshes;

		std::unordered_map<std::wstring, std::unique_ptr<AssimpModel>>* Models;
		MeshPass* SkyBoxMesh;

		std::unordered_map<std::wstring, std::unique_ptr<Shader>>* Shaders;
		std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D12PipelineState>>* PSOs;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC* BasePsoDesc;

		void DrawMeshes(ID3D12PipelineState* pso);
		void DrawMeshes(ID3D12PipelineState* opaqueStaticPSO, ID3D12PipelineState* opaqueSkinnedPSO, ID3D12PipelineState* transparentStaticPSO, ID3D12PipelineState* transparentSkinnedPSO);
		void DrawOpaqueMeshes(ID3D12PipelineState* opaqueStaticPSO, ID3D12PipelineState* opaqueSkinnedPSO);
		void DrawTransparentMeshes(ID3D12PipelineState* transparentStaticPSO, ID3D12PipelineState* transparentSkinnedPSO);
		void DrawSkyBox(ID3D12PipelineState* skyBoxPSO);
	};
}
