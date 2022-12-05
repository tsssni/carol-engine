#include "Model.h"
#include "../Manager/Mesh/Mesh.h"
#include "../DirectX/Heap.h"
#include "../Resource/Texture.h"

const unordered_map<wstring, unique_ptr<Carol::MeshManager>>& Carol::Model::GetMeshes()
{
	return mMeshes;
}