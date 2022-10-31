#pragma once
#include "../DirectX/Buffer.h"
#include "../DirectX/d3dx12.h"
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
	class Model
	{
	public:
		class Submesh
		{
		public:
			uint32_t BaseVertexLocation;
			uint32_t StartIndexLocation;
			uint32_t IndexCount;
			uint32_t MatTBIndex;
		};
	public:
		template<class Vertex, class Index>
		void LoadVerticesAndIndices(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, vector<Vertex>& vertices, vector<Index>& indices)
		{
			uint32_t vbByteSize = sizeof(Vertex) * vertices.size();
			uint32_t ibByteSize = sizeof(Index) * indices.size();
			uint32_t vertexStride = sizeof(Vertex);
			DXGI_FORMAT indexFormat;

			mVertexBufferCpu = make_unique<Blob>();
			mVertexBufferGpu = make_unique<DefaultBuffer>();
			mIndexBufferCpu = make_unique<Blob>();
			mIndexBufferGpu = make_unique<DefaultBuffer>();

			switch (sizeof(Index))
			{
			case 2: indexFormat = DXGI_FORMAT_R16_UINT; break;
			case 4: indexFormat = DXGI_FORMAT_R32_UINT; break;
			}

			mVertexBufferCpu->InitBlob(vertices.data(), vbByteSize);
			mIndexBufferCpu->InitBlob(indices.data(), ibByteSize);

			mVertexBufferGpu->InitDefaultBuffer(device, D3D12_HEAP_FLAG_NONE, GetRvaluePtr(CD3DX12_RESOURCE_DESC::Buffer(vbByteSize)));
			mVertexBufferGpu->CopySubresources(cmdList, GetRvaluePtr(CreateSingleSubresource(reinterpret_cast<void*>(vertices.data()), vbByteSize)));

			mIndexBufferGpu->InitDefaultBuffer(device, D3D12_HEAP_FLAG_NONE, GetRvaluePtr(CD3DX12_RESOURCE_DESC::Buffer(ibByteSize)));
			mIndexBufferGpu->CopySubresources(cmdList, GetRvaluePtr(CreateSingleSubresource(reinterpret_cast<void*>(indices.data()), ibByteSize)));

			mVertexBufferView = { mVertexBufferGpu->Get()->GetGPUVirtualAddress(), vbByteSize, vertexStride};
			mIndexBufferView = { mIndexBufferGpu->Get()->GetGPUVirtualAddress(), ibByteSize, indexFormat };
		}

		
		D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView();
		D3D12_INDEX_BUFFER_VIEW GetIndexBufferView();

		Submesh& GetSubmesh(wstring modelName);
		const unordered_map<wstring, Submesh>& GetSubmeshes();

	protected:
		virtual void InitSubmesh(
			wstring modelName,
			uint32_t baseVertexLocation,
			uint32_t startIndexLocation,
			uint32_t indexCount,
			uint32_t matTBIndex);


		unique_ptr<Blob> mVertexBufferCpu;
		unique_ptr<Blob> mIndexBufferCpu;
		unique_ptr<DefaultBuffer> mVertexBufferGpu;
		unique_ptr<DefaultBuffer> mIndexBufferGpu;

		D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
		D3D12_INDEX_BUFFER_VIEW mIndexBufferView;

		unordered_map<wstring, Submesh> mSubmeshes;
	};
}
