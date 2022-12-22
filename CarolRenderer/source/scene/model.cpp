#include <scene/model.h>
#include <render/mesh.h>

namespace Carol
{
	using std::unique_ptr;
	using std::unordered_map;
	using std::wstring;
}

const Carol::unordered_map<Carol::wstring, Carol::unique_ptr<Carol::MeshPass>>& Carol::Model::GetMeshes()
{
	return mMeshes;
}