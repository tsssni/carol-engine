#pragma once
#include <DirectXMath.h>

namespace Carol
{
	class Light
	{
    public:
        DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
        float FalloffStart = 1.0f;                          
        DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };
        float FalloffEnd = 10.0f;                  
        DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f }; 
        float SpotPower = 64.0f;                     
	};
}
