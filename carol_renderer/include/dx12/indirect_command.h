#pragma once
#include <d3d12.h>

namespace Carol
{
#pragma pack(1)
	class IndirectCommand
	{
	public:
		D3D12_GPU_VIRTUAL_ADDRESS MeshCBAddr;
		D3D12_GPU_VIRTUAL_ADDRESS SkinnedCBAddr;
		D3D12_DISPATCH_MESH_ARGUMENTS DispatchMeshArgs;
	};
#pragma pack()
}
