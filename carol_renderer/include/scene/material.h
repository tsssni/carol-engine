#pragma once
#include <DirectXMath.h>

namespace Carol
{
	class Material
	{
	public:
		DirectX::XMFLOAT3 Emissive = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 Ambient = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 Diffuse = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 Reflective = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 FresnelR0 = { 0.2f, 0.2f, 0.2f };
		float Roughness = 0.5f;
	};
}
