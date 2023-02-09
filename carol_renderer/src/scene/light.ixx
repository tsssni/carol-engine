export module carol.renderer.scene.light;
import <DirectXMath.h>;

namespace Carol {
    using namespace DirectX;

    export class Light
    {
    public:
        XMFLOAT3 Strength = { 0.3f, 0.3f, 0.3f };
        float FalloffStart = 1.0f;
        XMFLOAT3 Direction = { 1.0f, -1.0f, 1.0f };
        float FalloffEnd = 10.0f;
        XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
        float SpotPower = 64.0f;
        XMFLOAT3 Ambient = { 0.4f,0.4f,0.4f };
        float LightPad0;
        
        XMFLOAT4X4 View;
        XMFLOAT4X4 Proj;
        XMFLOAT4X4 ViewProj;
    };
}