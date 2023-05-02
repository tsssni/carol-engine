#include <carol.h>
#include <DirectXColors.h>
#include <DirectXPackedVector.h>
#include <string_view>

#define BORDER_RADIUS 5

namespace Carol
{
	using std::vector;
	using std::wstring;
	using std::wstring_view;
	using std::make_unique;
	using namespace DirectX;
	using namespace DirectX::PackedVector;
	using Microsoft::WRL::ComPtr;
}

Carol::SsaoPass::SsaoPass(
	uint32_t blurCount,
	DXGI_FORMAT ambientMapFormat)
	:mBlurCount(blurCount),
	mAmbientMapFormat(ambientMapFormat),
	mEpfPass(make_unique<EpfPass>())
{
	InitPSOs();
	InitRandomVectors();
	InitRandomVectorMap();
}

uint32_t Carol::SsaoPass::GetRandVecSrvIdx()const
{
	return mRandomVecMap->GetGpuSrvIdx();
}

uint32_t Carol::SsaoPass::GetSsaoSrvIdx()const
{
	return mAmbientMap->GetGpuSrvIdx();
}

uint32_t Carol::SsaoPass::GetSsaoUavIdx()const
{
	return mAmbientMap->GetGpuUavIdx();
}

void Carol::SsaoPass::ReleaseIntermediateBuffers()
{
	mRandomVecMap->ReleaseIntermediateBuffer();
}

void Carol::SsaoPass::OnResize(
	uint32_t width,
	uint32_t height)
{
	if (mWidth != width >> 1 || mHeight != height >> 1)
	{
		mWidth = width >> 1;
		mHeight = height >> 1;
		mViewport = { 0.f,0.f,mWidth * 1.f,mHeight * 1.f,0.f,1.f };
		mScissorRect = { 0,0,(long)mWidth,(long)mHeight };

		InitBuffers();
	}
}

void Carol::SsaoPass::Draw()
{
	uint32_t groupWidth = ceilf(mWidth * 1.f / 32.f);
	uint32_t groupHeight = ceilf(mHeight * 1.f / 32.f);

	mAmbientMap->Transition(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	gGraphicsCommandList->SetPipelineState(mSsaoComputePSO->Get());
	gGraphicsCommandList->Dispatch(groupWidth, groupHeight, 1);
	mAmbientMap->Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	mEpfPass->SetColorMap(mAmbientMap.get());
	mEpfPass->Draw();
}

Carol::vector<float> Carol::SsaoPass::CalcGaussWeights(float sigma)
{
	static int maxGaussRadius = 5;

	int blurRadius = (int)ceil(2.0f * sigma);
	assert(blurRadius <= maxGaussRadius);

	vector<float> weights;
	float weightsSum = 0.0f;

	weights.resize(2 * blurRadius + 1);

	for (int i = -blurRadius; i <= blurRadius; ++i)
	{
		weights[blurRadius + i] = expf(-i * i / (2.0f * sigma * sigma));
		weightsSum += weights[blurRadius + i];
	}

	for (int i = 0; i < blurRadius * 2 + 1; ++i)
	{
		weights[i] /= weightsSum;
	}

	return weights;
}

void Carol::SsaoPass::InitBuffers()
{
	mAmbientMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mAmbientMapFormat,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}

void Carol::SsaoPass::InitRandomVectors()
{
	mOffsets[0] = XMFLOAT4(+1.0f, +1.0f, +1.0f, 0.0f);
	mOffsets[1] = XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);

	mOffsets[2] = XMFLOAT4(-1.0f, +1.0f, +1.0f, 0.0f);
	mOffsets[3] = XMFLOAT4(+1.0f, -1.0f, -1.0f, 0.0f);

	mOffsets[4] = XMFLOAT4(+1.0f, +1.0f, -1.0f, 0.0f);
	mOffsets[5] = XMFLOAT4(-1.0f, -1.0f, +1.0f, 0.0f);

	mOffsets[6] = XMFLOAT4(-1.0f, +1.0f, -1.0f, 0.0f);
	mOffsets[7] = XMFLOAT4(+1.0f, -1.0f, +1.0f, 0.0f);

	// 6 centers of cube faces
	mOffsets[8] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
	mOffsets[9] = XMFLOAT4(+1.0f, 0.0f, 0.0f, 0.0f);

	mOffsets[10] = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
	mOffsets[11] = XMFLOAT4(0.0f, +1.0f, 0.0f, 0.0f);

	mOffsets[12] = XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
	mOffsets[13] = XMFLOAT4(0.0f, 0.0f, +1.0f, 0.0f);

	for (int i = 0; i < 14; ++i)
	{
		float s = 0.25f + rand() * 1.0f / RAND_MAX * (1.0f - 0.25f);
		XMVECTOR v = s * XMVector4Normalize(XMLoadFloat4(&mOffsets[i]));

		XMStoreFloat4(&mOffsets[i], v);
	}
}

void Carol::SsaoPass::InitRandomVectorMap()
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
}

void Carol::SsaoPass::InitPSOs()
{
	mSsaoComputePSO = make_unique<ComputePSO>(PSO_DEFAULT);
	mSsaoComputePSO->SetRootSignature(sRootSignature.get());
	mSsaoComputePSO->SetCS(gSsaoCS.get());
	mSsaoComputePSO->Finalize();
}

void Carol::SsaoPass::GetOffsetVectors(XMFLOAT4 offsets[14])
{
	std::copy(&mOffsets[0], &mOffsets[14], offsets);
}