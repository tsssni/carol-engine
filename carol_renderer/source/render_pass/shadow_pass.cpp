#include <global.h>
#include <DirectXColors.h>
#include <memory>
#include <string_view>

namespace Carol {
	using std::vector;
	using std::wstring;
	using std::wstring_view;
	using std::unique_ptr;
	using std::make_unique;
	using std::span;
	using namespace DirectX;
}

Carol::ShadowPass::ShadowPass(
	Light light,
	uint32_t width,
	uint32_t height,
	uint32_t depthBias,
	float depthBiasClamp,
	float slopeScaledDepthBias,
	DXGI_FORMAT shadowFormat,
	DXGI_FORMAT hiZFormat)
	:mLight(make_unique<Light>(light)),
	mDepthBias(depthBias),
	mDepthBiasClamp(depthBiasClamp),
	mSlopeScaledDepthBias(slopeScaledDepthBias),
	mShadowFormat(shadowFormat)
{
	InitLight();
	InitPSOs();

	mCullPass = make_unique<CullPass>(
		depthBias,
		depthBiasClamp,
		slopeScaledDepthBias,
		shadowFormat);

	OnResize(
		width,
		height);
}

void Carol::ShadowPass::Draw()
{
	mCullPass->Draw();
}

void Carol::ShadowPass::Update(uint32_t lightIdx)
{
	XMMATRIX view = mCamera->GetView();
	XMMATRIX proj = mCamera->GetProj();
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX histViewProj = XMMatrixTranspose(XMLoadFloat4x4(&mLight->ViewProj));
	
	XMStoreFloat4x4(&mLight->View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mLight->Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mLight->ViewProj, XMMatrixTranspose(viewProj));

	mCullPass->Update(XMMatrixTranspose(viewProj), XMMatrixTranspose(histViewProj), XMLoadFloat3(&mLight->Position));
}

uint32_t Carol::ShadowPass::GetShadowSrvIdx()const
{
	return mShadowMap->GetGpuSrvIdx();
}

const Carol::Light& Carol::ShadowPass::GetLight()const
{
	return *mLight;
}

void Carol::ShadowPass::InitBuffers()
{
	D3D12_CLEAR_VALUE optClearValue;
	optClearValue.Format = GetDsvFormat(mShadowFormat);
	optClearValue.DepthStencil.Depth = 1.0f;
	optClearValue.DepthStencil.Stencil = 0;

	mShadowMap = make_unique<ColorBuffer>(
		mWidth,
		mHeight,
		1,
		COLOR_BUFFER_VIEW_DIMENSION_TEXTURE2D,
		mShadowFormat,
		gHeapManager->GetDefaultBuffersHeap(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		&optClearValue);

	mCullPass->OnResize(mWidth, mHeight);
	mCullPass->SetDepthMap(mShadowMap.get());
}

void Carol::ShadowPass::InitLight()
{
	mViewport.TopLeftX = 0.0f;
	mViewport.TopLeftY = 0.0f;
	mViewport.Width = mWidth;
	mViewport.Height = mHeight;
	mViewport.MinDepth = 0.0f;
	mViewport.MaxDepth = 1.0f;
	mScissorRect = { 0,0,(int)mWidth,(int)mHeight };
}

void Carol::ShadowPass::InitPSOs()
{
}

Carol::DirectLightShadowPass::DirectLightShadowPass(
	Light light,
	uint32_t width,
	uint32_t height,
	uint32_t depthBias,
	float depthBiasClamp,
	float slopeScaledDepthBias,
	DXGI_FORMAT shadowFormat,
	DXGI_FORMAT hiZFormat)
	:ShadowPass(
		light,
		width,
		height,
		depthBias,
		depthBiasClamp,
		slopeScaledDepthBias,
		shadowFormat,
		hiZFormat)
{
	InitCamera();
}

void Carol::DirectLightShadowPass::Update(
	uint32_t lightIdx,
	const PerspectiveCamera* camera,
	float zn,
	float zf)
{
	static float dx[4] = { -1.f,1.f,-1.f,1.f };
	static float dy[4] = { -1.f,-1.f,1.f,1.f };

	XMFLOAT4 pointNear;
	pointNear.z = zn;
	pointNear.y = zn * tanf(0.5f * camera->GetFovY());
	pointNear.x = zn * tanf(0.5f * camera->GetFovX());
	
	XMFLOAT4 pointFar;
	pointFar.z = zf;
	pointFar.y = zf * tanf(0.5f * camera->GetFovY());
	pointFar.x = zf * tanf(0.5f * camera->GetFovX());

	vector<XMFLOAT4> frustumSliceExtremaPoints;
	XMMATRIX perspView = camera->GetView();
	XMMATRIX invPerspView = XMMatrixInverse(GetRvaluePtr(XMMatrixDeterminant(perspView)), perspView);
	XMMATRIX orthoView = mCamera->GetView();
	XMMATRIX invOrthoView = XMMatrixInverse(GetRvaluePtr(XMMatrixDeterminant(orthoView)), orthoView);
	XMMATRIX invPerspOrthoView = XMMatrixMultiply(invPerspView, orthoView);

	for (int i = 0; i < 4; ++i)
	{
		XMFLOAT4 point;
		
		point = { pointNear.x* dx[i], pointNear.y* dy[i], pointNear.z, 1.f };
		XMStoreFloat4(&point, XMVector4Transform(XMLoadFloat4(&point), invPerspOrthoView));
		frustumSliceExtremaPoints.push_back(point);

		point = { pointFar.x* dx[i], pointFar.y* dy[i], pointFar.z, 1.f };
		XMStoreFloat4(&point, XMVector4Transform(XMLoadFloat4(&point), invPerspOrthoView));
		frustumSliceExtremaPoints.push_back(point);
	}

	XMFLOAT4 boxMin = { D3D12_FLOAT32_MAX,D3D12_FLOAT32_MAX,D3D12_FLOAT32_MAX,1.f };
	XMFLOAT4 boxMax = { -D3D12_FLOAT32_MAX,-D3D12_FLOAT32_MAX,-D3D12_FLOAT32_MAX,1.f };

	for (auto& point : frustumSliceExtremaPoints)
	{
		boxMin.x = std::min(boxMin.x, point.x);
		boxMin.y = std::min(boxMin.y, point.y);
		boxMin.z = std::min(boxMin.z, point.z);

		boxMax.x = std::max(boxMax.x, point.x);
		boxMax.y = std::max(boxMax.y, point.y);
		boxMax.z = std::max(boxMax.z, point.z);
	}
	
	XMFLOAT4 center;
	XMStoreFloat4(&center, XMVector4Transform(0.5f * (XMLoadFloat4(&boxMin) + XMLoadFloat4(&boxMax)), invOrthoView));

	dynamic_cast<OrthographicCamera*>(mCamera.get())->SetLens(boxMax.x - boxMin.x, boxMax.y - boxMin.y, 1.f, boxMax.z + boxMax.z - boxMin.z);
	mCamera->LookAt(XMLoadFloat4(&center) - 140.f * XMLoadFloat3(&mLight->Direction), XMLoadFloat4(&center), { 0.f,1.f,0.f,0.f });
	mCamera->UpdateViewMatrix();

	ShadowPass::Update(lightIdx);
}

void Carol::DirectLightShadowPass::InitCamera()
{
	mLight->Position = XMFLOAT3(-mLight->Direction.x * 140.f, -mLight->Direction.y * 140.f, -mLight->Direction.z * 140.f);
	mCamera = make_unique<OrthographicCamera>(70, 0, 280);
	mCamera->LookAt(mLight->Position, { 0.f,0.f,0.f }, { 0.f,1.f,0.f });
	mCamera->UpdateViewMatrix();
}

Carol::CascadedShadowPass::CascadedShadowPass(
	Light light,
	uint32_t splitLevel,
	uint32_t width,
	uint32_t height,
	uint32_t depthBias,
	float depthBiasClamp,
	float slopeScaledDepthBias,
	DXGI_FORMAT shadowFormat,
	DXGI_FORMAT hiZFormat)
	:mSplitLevel(splitLevel),
	mSplitZ(splitLevel + 1),
	mShadow(splitLevel)
{
	for (auto& shadow : mShadow)
	{
		shadow = make_unique<DirectLightShadowPass>(
			light,
			width,
			height,
			depthBias,
			depthBiasClamp,
			slopeScaledDepthBias,
			shadowFormat,
			hiZFormat);
	}
}

void Carol::CascadedShadowPass::Draw()
{
	for (auto& shadow : mShadow)
	{
		shadow->Draw();
	}
}

void Carol::CascadedShadowPass::Update(
	const PerspectiveCamera* camera,
	float logWeight,
	float bias)
{
	float nearZ = camera->GetNearZ();
	float farZ = camera->GetFarZ();

	for (int i = 0; i < mSplitLevel + 1; ++i)
	{
		mSplitZ[i] = logWeight * nearZ * pow(farZ / nearZ, 1.f * i / mSplitLevel) + (1 - logWeight) * (nearZ + (farZ - nearZ) * (1.f * i / mSplitLevel)) + bias;
	}

	for (int i = 0; i < mSplitLevel; ++i)
	{
		mShadow[i]->Update(i, camera, mSplitZ[i], mSplitZ[i + 1]);
	}
}

uint32_t Carol::CascadedShadowPass::GetSplitLevel()const
{
	return mSplitLevel;
}

float Carol::CascadedShadowPass::GetSplitZ(uint32_t idx)const
{
	return mSplitZ[idx];
}

uint32_t Carol::CascadedShadowPass::GetShadowSrvIdx(uint32_t idx)const
{
	return mShadow[idx]->GetShadowSrvIdx();
}

const Carol::Light& Carol::CascadedShadowPass::GetLight(uint32_t idx)const
{
	return mShadow[idx]->GetLight();
}

void Carol::CascadedShadowPass::InitPSOs()
{
}

void Carol::CascadedShadowPass::InitBuffers()
{
}