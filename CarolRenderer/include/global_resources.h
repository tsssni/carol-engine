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
	class Shader;
	class CircularHeap;
	class DescriptorAllocator;
	class DisplayManager;
	class LightManager;
	class SsaoManager;
	class TaaManager;
	class OitppllManager;
	class BoundingRect;
	class Camera;
	class Timer;
	class AssimpModel;
	class MeshManager;
	class PassConstants;

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

		DescriptorAllocator* CbvSrvUavAllocator;
		DescriptorAllocator* RtvAllocator;
		DescriptorAllocator* DsvAllocator;

		DisplayManager* Display;
		LightManager* MainLight;
		SsaoManager* Ssao;
		TaaManager* Taa;
		OitppllManager* Oitppll;

		D3D12_VIEWPORT* ScreenViewport;
		D3D12_RECT* ScissorRect;

		Camera* Camera;
		Timer* Timer;

		uint32_t* ClientWidth;
		uint32_t* ClientHeight;

		ID3D12RootSignature* RootSignature;
		std::vector<MeshManager*>* OpaqueStaticMeshes;
		std::vector<MeshManager*>* OpaqueSkinnedMeshes;
		std::vector<MeshManager*>* TransparentStaticMeshes;
		std::vector<MeshManager*>* TransparentSkinnedMeshes;

		std::unordered_map<std::wstring, std::unique_ptr<AssimpModel>>* Models;
		MeshManager* SkyBoxMesh;

		std::unordered_map<std::wstring, std::unique_ptr<Shader>>* Shaders;
		std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D12PipelineState>>* PSOs;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC* BasePsoDesc;

		void DrawMeshes(ID3D12PipelineState* pso, bool textureDrawing);
		void DrawMeshes(ID3D12PipelineState* opaqueStaticPSO, ID3D12PipelineState* opaqueSkinnedPSO, ID3D12PipelineState* transparentStaticPSO, ID3D12PipelineState* transparentSkinnedPSO, bool textureDrawing);
		void DrawOpaqueMeshes(ID3D12PipelineState* opaqueStaticPSO, ID3D12PipelineState* opaqueSkinnedPSO, bool textureDrawing);
		void DrawTransparentMeshes(ID3D12PipelineState* transparentStaticPSO, ID3D12PipelineState* transparentSkinnedPSO, bool textureDrawing);
		void DrawSkyBox(ID3D12PipelineState* skyBoxPSO);
	};
}
