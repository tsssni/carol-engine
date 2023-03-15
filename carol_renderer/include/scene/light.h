#pragma once
#include <DirectXMath.h>

namespace Carol {
    enum LightType
    {
        DIRECTIONAL_LIGHT,
        POINT_LIGHT,
        SPOT_LIGHT,
        AREA_LIGHT,
        LIGHT_TYPE_COUNT
    };

    class Light
    {
    public:
        DirectX::XMFLOAT3 Strength = { 0.3f, 0.3f, 0.3f };
        float LightPad0;
        DirectX::XMFLOAT3 Direction = { 1.0f, -1.0f, 1.0f };
        float LightPad1;
        DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
        float LightPad2;

		float AttenuationConstant = 0.f;
		float AttenuationLinear = 0.f;
		float AttenuationQuadric= 1.f;
        float LightPad3;

        float SpotInnerAngle = 0.f;
        float SpotOuterAngle = 0.f;
        DirectX::XMFLOAT2 AreaSize = { 0.f,0.f };

        DirectX::XMFLOAT4X4 View;
        DirectX::XMFLOAT4X4 Proj;
        DirectX::XMFLOAT4X4 ViewProj;
    };
}


