#pragma once
#include <DirectXMath.h>

namespace Carol
{
	class MaterialData
	{
	public:
		DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f,1.0f,1.0f,1.0f };
		DirectX::XMFLOAT3 FresnelR0 = { 0.01f,0.01f,0.01f };
		float Roughness = 0.0f;
		DirectX::XMFLOAT4X4 MatTransform;
		
		int DiffuseSrvHeapIndex = -1;
		int NormalSrvHeapIndex = -1;
		int MatPad0 = 0;
		int MatPad1 = 0;
	};
}


