#pragma once
#include "../DirectX/Resource.h"
#include "../DirectX/DescriptorAllocator.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>

using Microsoft::WRL::ComPtr;
using std::vector;
using std::unique_ptr;
using std::unordered_map;
using std::wstring;

namespace Carol
{
	class Heap;
	class Shader;
	class CircularHeap;
	class DiscriptorAllocator;
	class DisplayManager;
	class LightManager;
	class SsaoManager;
	class TaaManager;
	class OitppllManager;
	class BoundingRect;
	class Camera;
	class GameTimer;
	class AssimpModel;
	class MeshManager;
	class PassConstants;

	class RenderData
    {
	public:
		ID3D12Device* Device;
		ID3D12CommandQueue* CommandQueue;
		ID3D12GraphicsCommandList* CommandList;

		Heap* DefaultBuffersHeap;
		Heap* UploadBuffersHeap;
		Heap* ReadbackBuffersHeap;
		Heap* SrvTexturesHeap;
		Heap* RtvDsvTexturesHeap;

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
		GameTimer* Timer;

		uint32_t* ClientWidth;
		uint32_t* ClientHeight;

		ID3D12RootSignature* RootSignature;
		vector<MeshManager*>* OpaqueStaticMeshes;
		vector<MeshManager*>* OpaqueSkinnedMeshes;
		vector<MeshManager*>* TransparentStaticMeshes;
		vector<MeshManager*>* TransparentSkinnedMeshes;

		unordered_map<wstring, unique_ptr<AssimpModel>>* Models;
		MeshManager* SkyBoxMesh;

		unordered_map<wstring, unique_ptr<Shader>>* Shaders;
		unordered_map<wstring, ComPtr<ID3D12PipelineState>>* PSOs;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC* BasePsoDesc;

		void DrawMeshes(ID3D12PipelineState* pso, bool textureDrawing);
		void DrawMeshes(ID3D12PipelineState* opaqueStaticPSO, ID3D12PipelineState* opaqueSkinnedPSO, ID3D12PipelineState* transparentStaticPSO, ID3D12PipelineState* transparentSkinnedPSO, bool textureDrawing);
		void DrawOpaqueMeshes(ID3D12PipelineState* opaqueStaticPSO, ID3D12PipelineState* opaqueSkinnedPSO, bool textureDrawing);
		void DrawTransparentMeshes(ID3D12PipelineState* transparentStaticPSO, ID3D12PipelineState* transparentSkinnedPSO, bool textureDrawing);
		void DrawSkyBox(ID3D12PipelineState* skyBoxPSO);
    };

	class Manager
	{
	public:
		Manager(RenderData* renderData);
		Manager(const Manager&) = delete;
		Manager(Manager&&) = delete;
		Manager& operator=(const Manager&) = delete;
		~Manager();

		virtual void Draw() = 0;
		virtual void Update() = 0;
		virtual void OnResize();
		virtual void ReleaseIntermediateBuffers() = 0;
		virtual void CopyDescriptors();
	protected:
		virtual void InitRootSignature() = 0;
		virtual void InitShaders() = 0;
		virtual void InitPSOs() = 0;
		virtual void InitResources() = 0;
		virtual void InitDescriptors() = 0;

		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtv(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsv(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetCbv(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuSrv(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetShaderCpuSrv(int idx);
		virtual CD3DX12_GPU_DESCRIPTOR_HANDLE GetShaderGpuSrv(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuUav(int idx);
		virtual CD3DX12_CPU_DESCRIPTOR_HANDLE GetShaderCpuUav(int idx);
		virtual CD3DX12_GPU_DESCRIPTOR_HANDLE GetShaderGpuUav(int idx);


		RenderData* mRenderData;
		ComPtr<ID3D12RootSignature> mRootSignature;

		unique_ptr<DescriptorAllocInfo> mCpuCbvSrvUavAllocInfo;
		unique_ptr<DescriptorAllocInfo> mGpuCbvSrvUavAllocInfo;
		unique_ptr<DescriptorAllocInfo> mRtvAllocInfo;
		unique_ptr<DescriptorAllocInfo> mDsvAllocInfo;
	};
}
