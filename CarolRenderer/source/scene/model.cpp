#include <scene/model.h>
#include <manager/mesh.h>

namespace Carol
{
	using std::unique_ptr;
	using std::unordered_map;
	using std::wstring;
}

const Carol::unordered_map<Carol::wstring, Carol::unique_ptr<Carol::MeshManager>>& Carol::Model::GetMeshes()
{
	return mMeshes;
}