#include "FrameResources.h"
#include "Utils/Common.h"

Carol::FrameResource::FrameResource(
	ID3D12Device* device,
	int numPassCB,
	int numObjCB,
	int numSkinnedCB)
{
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(FRCommandAllocator.GetAddressOf())));

	PassCB.InitUploadBuffer(device, D3D12_HEAP_FLAG_NONE, numPassCB, true);
	ObjCB.InitUploadBuffer(device, D3D12_HEAP_FLAG_NONE, numObjCB, true);
	SkinnedCB.InitUploadBuffer(device, D3D12_HEAP_FLAG_NONE, numSkinnedCB, true);
	SsaoCB.InitUploadBuffer(device, D3D12_HEAP_FLAG_NONE, 1, true);
}
