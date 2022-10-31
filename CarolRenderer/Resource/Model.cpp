#include "Model.h"
#include <utility>

void Carol::Model::InitSubmesh(
	wstring modelName,
	uint32_t baseVertexLocation,
	uint32_t startIndexLocation,
	uint32_t indexCount,
	uint32_t matTBIndex)
{	
	mSubmeshes[modelName] = { baseVertexLocation, startIndexLocation, indexCount, matTBIndex};
}

D3D12_VERTEX_BUFFER_VIEW Carol::Model::GetVertexBufferView()
{
	return mVertexBufferView;
}

D3D12_INDEX_BUFFER_VIEW Carol::Model::GetIndexBufferView()
{
	return mIndexBufferView;
}

const unordered_map<wstring, Carol::Model::Submesh>& Carol::Model::GetSubmeshes()
{
	return mSubmeshes;
}

Carol::Model::Submesh& Carol::Model::GetSubmesh(wstring modelName)
{
	return mSubmeshes[modelName];
}
