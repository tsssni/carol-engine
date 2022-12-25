#pragma once
#include <DirectXMath.h>

namespace Carol {
    enum
    {
        DIR_LIGHT, POINT_LIGHT, SPOT_LIGHT
    };

    class Light
    {
    public:
        DirectX::XMFLOAT3 Strength = { 0.3f, 0.3f, 0.3f };
        float FalloffStart = 1.0f;
        DirectX::XMFLOAT3 Direction = { 1.0f, -1.0f, 1.0f };
        float FalloffEnd = 10.0f;
        DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
        float SpotPower = 64.0f;
        DirectX::XMFLOAT3 Ambient = { 0.4f,0.4f,0.4f };
        float LightPad0;
        
        DirectX::XMFLOAT4X4 ViewProj;
        DirectX::XMFLOAT4X4 ViewProjTex;
    };
}


