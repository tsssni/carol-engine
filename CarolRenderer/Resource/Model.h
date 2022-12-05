#pragma once
#include "../Manager/Manager.h"
#include "../DirectX/Resource.h"
#include "../Utils/d3dx12.h"
#include <d3d12.h>
#include <DirectXCollision.h>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

using std::vector;
using std::unordered_map;
using std::wstring;
using std::unique_ptr;
using std::make_unique;

namespace Carol
{
	class MeshManager;

	class Model
	{
	public:
		template<class Vertex, class Index>
		void LoadVerticesAndIndices(RenderData* renderData, vector<Vertex>& vertices, vector<Index>& indices)
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

			mVertexBufferGpu = make_unique<DefaultResource>(GetRvaluePtr(CD3DX12_RESOURCE_DESC::Buffer(vbByteSize)), renderData->DefaultBuffersHeap);
			mVertexBufferGpu->CopySubresources(renderData, GetRvaluePtr(CreateSingleSubresource(reinterpret_cast<void*>(vertices.data()), vbByteSize)));

			mIndexBufferGpu = make_unique<DefaultResource>(GetRvaluePtr(CD3DX12_RESOURCE_DESC::Buffer(ibByteSize)), renderData->DefaultBuffersHeap);
			mIndexBufferGpu->CopySubresources(renderData, GetRvaluePtr(CreateSingleSubresource(reinterpret_cast<void*>(indices.data()), ibByteSize)));

			mVertexBufferView = { mVertexBufferGpu->Get()->GetGPUVirtualAddress(), vbByteSize, vertexStride};
			mIndexBufferView = { mIndexBufferGpu->Get()->GetGPUVirtualAddress(), ibByteSize, indexFormat };
		}

		const unordered_map<wstring, unique_ptr<MeshManager>>& GetMeshes();
	protected:
		unique_ptr<DefaultResource> mVertexBufferGpu;
		unique_ptr<DefaultResource> mIndexBufferGpu;

		D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
		D3D12_INDEX_BUFFER_VIEW mIndexBufferView;

		unordered_map<wstring, unique_ptr<MeshManager>> mMeshes;
	};
}
