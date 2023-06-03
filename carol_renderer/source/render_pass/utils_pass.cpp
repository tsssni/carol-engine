#include <render_pass/utils_pass.h>
#include <dx12/heap.h>
#include <dx12/resource.h>
#include <global.h>
#include <DirectXColors.h>
#include <DirectXPackedVector.h>

namespace Carol
{
	using std::make_unique;
	using namespace DirectX;
	using namespace DirectX::PackedVector;
}

Carol::UtilsPass::UtilsPass()
{
	InitBuffers();
}

void Carol::UtilsPass::Draw()
{
}

uint32_t Carol::UtilsPass::GetRandVecSrvIdx() const
{
	return mRandomVecMap->GetGpuSrvIdx();
}

void Carol::UtilsPass::InitPSOs()
{
}

void Carol::UtilsPass::InitBuffers()
{
	mRandomVecMap = make_unique<ColorBuffer>(
		256,
		256,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	XMCOLOR initData[256 * 256];
	for (int i = 0; i < 256; ++i)
	{
		for (int j = 0; j < 256; ++j)
		{
			XMCOLOR randVec = { rand() * 1.0f / RAND_MAX,rand() * 1.0f / RAND_MAX,rand() * 1.0f / RAND_MAX ,0.0f };
			initData[i * 256 + j] = randVec;
		}
	}

	D3D12_SUBRESOURCE_DATA subresource;
	subresource.pData = initData;
	subresource.RowPitch = 256 * sizeof(XMCOLOR);
	subresource.SlicePitch = subresource.RowPitch * 256;

	mRandomVecMap->CopySubresources(gHeapManager->GetUploadBuffersHeap(), &subresource, 0, 1);
	mRandomVecMap->ReleaseIntermediateBuffer();
}