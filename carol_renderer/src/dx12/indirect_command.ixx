export module carol.renderer.dx12.indirect_command;
import carol.renderer.utils;

#pragma pack(1)
	export class IndirectCommand
	{
	public:
		D3D12_GPU_VIRTUAL_ADDRESS MeshCBAddr;
		D3D12_GPU_VIRTUAL_ADDRESS SkinnedCBAddr;
		D3D12_DISPATCH_MESH_ARGUMENTS DispatchMeshArgs;
	};
#pragma pack()