#pragma once

namespace Carol
{
	enum MeshType
	{
		OPAQUE_STATIC,
		OPAQUE_SKINNED,
		TRANSPARENT_STATIC,
		TRANSPARENT_SKINNED,
		MESH_TYPE_COUNT
	};

	enum OpaqueMeshType
	{
		OPAQUE_MESH_START = 0,
		OPAQUE_MESH_TYPE_COUNT = 2
	};

	enum TransparentMeshType
	{
		TRANSPARENT_MESH_START = 2,
		TRANSPARENT_MESH_TYPE_COUNT = 2
	};
}

