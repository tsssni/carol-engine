#include <render_pass/shadow_pass.h>
#include <render_pass/cull_pass.h>
#include <dx12/heap.h>
#include <dx12/resource.h>
#include <dx12/pipeline_state.h>
#include <scene/camera.h>
#include <global.h>
#include <DirectXColors.h>
#include <memory>
#include <string_view>

namespace
{
	using DirectX::operator+;
	using DirectX::operator-;
	using DirectX::operator*;
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
	:mLight(std::make_unique<Light>(light)),
	mDepthBias(depthBias),
	mDepthBiasClamp(depthBiasClamp),
	mSlopeScaledDepthBias(slopeScaledDepthBias),
	mShadowFormat(shadowFormat)
{
	InitLight();
	InitPSOs();

	mCullPass = std::make_unique<CullPass>(
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
	DirectX::XMMATRIX view = mCamera->GetView();
	DirectX::XMMATRIX proj = mCamera->GetProj();
	DirectX::XMMATRIX viewProj = DirectX::XMMatrixMultiply(view, proj);
	DirectX::XMMATRIX histViewProj = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&mLight->ViewProj));
	
	DirectX::XMStoreFloat4x4(&mLight->View, DirectX::XMMatrixTranspose(view));
	DirectX::XMStoreFloat4x4(&mLight->Proj, DirectX::XMMatrixTranspose(proj));
	DirectX::XMStoreFloat4x4(&mLight->ViewProj, DirectX::XMMatrixTranspose(viewProj));

	mCullPass->Update(DirectX::XMMatrixTranspose(viewProj), DirectX::XMMatrixTranspose(histViewProj), DirectX::XMLoadFloat3(&mLight->Position));
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

	mShadowMap = std::make_unique<ColorBuffer>(
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
	const PerspectiveCamera* eyeCamera,
	float zn,
	float zf)
{
	static float dx[4] = { -1.f,1.f,-1.f,1.f };
	static float dy[4] = { -1.f,-1.f,1.f,1.f };

	DirectX::XMFLOAT4 pointNear;
	pointNear.z = zn;
	pointNear.y = zn * tanf(0.5f * eyeCamera->GetFovY());
	pointNear.x = zn * tanf(0.5f * eyeCamera->GetFovX());
	
	DirectX::XMFLOAT4 pointFar;
	pointFar.z = zf;
	pointFar.y = zf * tanf(0.5f * eyeCamera->GetFovY());
	pointFar.x = zf * tanf(0.5f * eyeCamera->GetFovX());

	std::vector<DirectX::XMFLOAT4> frustumSliceExtremaPoints;
	DirectX::XMMATRIX perspView = eyeCamera->GetView();
	DirectX::XMMATRIX invPerspView = DirectX::XMMatrixInverse(nullptr, perspView);
	DirectX::XMMATRIX orthoView = mCamera->GetView();
	DirectX::XMMATRIX invOrthoView = DirectX::XMMatrixInverse(nullptr, orthoView);
	DirectX::XMMATRIX invPerspOrthoView = DirectX::XMMatrixMultiply(invPerspView, orthoView);

	for (int i = 0; i < 4; ++i)
	{
		DirectX::XMFLOAT4 point;
		
		point = { pointNear.x* dx[i], pointNear.y* dy[i], pointNear.z, 1.f };
		DirectX::XMStoreFloat4(&point, DirectX::XMVector4Transform(DirectX::XMLoadFloat4(&point), invPerspOrthoView));
		frustumSliceExtremaPoints.push_back(point);

		point = { pointFar.x* dx[i], pointFar.y* dy[i], pointFar.z, 1.f };
		DirectX::XMStoreFloat4(&point, DirectX::XMVector4Transform(DirectX::XMLoadFloat4(&point), invPerspOrthoView));
		frustumSliceExtremaPoints.push_back(point);
	}

	DirectX::XMFLOAT4 boxMin = { D3D12_FLOAT32_MAX,D3D12_FLOAT32_MAX,D3D12_FLOAT32_MAX,1.f };
	DirectX::XMFLOAT4 boxMax = { -D3D12_FLOAT32_MAX,-D3D12_FLOAT32_MAX,-D3D12_FLOAT32_MAX,1.f };

	for (auto& point : frustumSliceExtremaPoints)
	{
		boxMin.x = std::fmin(boxMin.x, point.x);
		boxMin.y = std::fmin(boxMin.y, point.y);
		boxMin.z = std::fmin(boxMin.z, point.z);

		boxMax.x = std::fmax(boxMax.x, point.x);
		boxMax.y = std::fmax(boxMax.y, point.y);
		boxMax.z = std::fmax(boxMax.z, point.z);
	}
	
	DirectX::XMFLOAT4 center;
	DirectX::XMStoreFloat4(&center, DirectX::XMVector4Transform(0.5f * (DirectX::XMLoadFloat4(&boxMin) + DirectX::XMLoadFloat4(&boxMax)), invOrthoView));

	float zRange = boxMax.z - boxMin.z;
	dynamic_cast<OrthographicCamera*>(mCamera.get())->SetLens(
		boxMax.x - boxMin.x,
		boxMax.y - boxMin.y,
		boxMin.z - zRange * 4,
		boxMax.z + zRange);

	mCamera->LookAt(DirectX::XMLoadFloat4(&center) - DirectX::XMLoadFloat3(&mLight->Direction), DirectX::XMLoadFloat4(&center), { 0.f,1.f,0.f,0.f });
	mCamera->UpdateViewMatrix();

	ShadowPass::Update(lightIdx);
}

void Carol::DirectLightShadowPass::InitCamera()
{
	mLight->Position = DirectX::XMFLOAT3(-mLight->Direction.x, -mLight->Direction.y, -mLight->Direction.z);
	mCamera = std::make_unique<OrthographicCamera>(50, 0, 200);
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
	mSplitZ(splitLevel),
	mShadow(splitLevel)
{
	for (auto& shadow : mShadow)
	{
		shadow = std::make_unique<DirectLightShadowPass>(
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
	const PerspectiveCamera* eyeCamera,
	float logWeight,
	float bias)
{
	float nearZ = eyeCamera->GetNearZ();
	float farZ = eyeCamera->GetFarZ();

	for (int i = 1; i <= mSplitLevel; ++i)
	{
		mSplitZ[i - 1] = logWeight * nearZ * pow(farZ / nearZ, 1.f * i / mSplitLevel) + (1 - logWeight) * (nearZ + (farZ - nearZ) * (1.f * i / mSplitLevel)) + bias;
	}

	for (int i = 0; i < mSplitLevel; ++i)
	{
		mShadow[i]->Update(i, eyeCamera, i == 0 ? 0.f : mSplitZ[i - 1], mSplitZ[i]);
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