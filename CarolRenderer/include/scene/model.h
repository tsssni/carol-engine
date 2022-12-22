#pragma once
#include <render/global_resources.h>
#include <dx12/resource.h>
#include <utils/d3dx12.h>
#include <d3d12.h>
#include <DirectXCollision.h>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace Carol
{
	class MeshPass;

	class Model
	{
	public:
		template<class Vertex, class Index>
		void LoadVerticesAndIndices(ID3D12GraphicsCommandList* cmdList, Heap* heap, Heap* uploadHeap, std::vector<Vertex>& vertices, std::vector<Index>& indices)
		{
			uint32_t vbByteSize = sizeof(Vertex) * vertices.size();
			uint32_t ibByteSize = sizeof(Index) * indices.size();
			uint32_t vertexStride = sizeof(Vertex);
			DXGI_FORMAT indexFormat;

			switch (sizeof(Index))
			{
			case 2: indexFormat = DXGI_FORMAT_R16_UINT; break;
			case 4: indexFormat = DXGI_FORMAT_R32_UINT; break;
			}

			mVertexBufferGpu = std::make_unique<DefaultResource>(GetRvaluePtr(CD3DX12_RESOURCE_DESC::Buffer(vbByteSize)), heap);
			mVertexBufferGpu->CopySubresources(cmdList, uploadHeap, GetRvaluePtr(CreateSingleSubresource(reinterpret_cast<void*>(vertices.data()), vbByteSize)));

			mIndexBufferGpu = std::make_unique<DefaultResource>(GetRvaluePtr(CD3DX12_RESOURCE_DESC::Buffer(ibByteSize)), heap);
			mIndexBufferGpu->CopySubresources(cmdList, uploadHeap, GetRvaluePtr(CreateSingleSubresource(reinterpret_cast<void*>(indices.data()), ibByteSize)));

			mVertexBufferView = { mVertexBufferGpu->Get()->GetGPUVirtualAddress(), vbByteSize, vertexStride};
			mIndexBufferView = { mIndexBufferGpu->Get()->GetGPUVirtualAddress(), ibByteSize, indexFormat };
		}

		const std::unordered_map<std::wstring, std::unique_ptr<MeshPass>>& GetMeshes();
	protected:
		std::unique_ptr<DefaultResource> mVertexBufferGpu;
		std::unique_ptr<DefaultResource> mIndexBufferGpu;

		D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
		D3D12_INDEX_BUFFER_VIEW mIndexBufferView;

		std::unordered_map<std::wstring, std::unique_ptr<MeshPass>> mMeshes;
	};
}
