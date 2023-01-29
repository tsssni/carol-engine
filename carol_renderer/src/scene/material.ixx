export module carol.renderer.scene.material;
import <DirectXMath.h>;

namespace Carol
{
    using DirectX::XMFLOAT3;

    export class Material
	{
	public:
		XMFLOAT3 Emissive = { 0.0f, 0.0f, 0.0f };
		XMFLOAT3 Ambient = { 0.0f, 0.0f, 0.0f };
		XMFLOAT3 Diffuse = { 0.0f, 0.0f, 0.0f };
		XMFLOAT3 Reflective = { 0.0f, 0.0f, 0.0f };
		XMFLOAT3 FresnelR0 = { 0.2f, 0.2f, 0.2f };
		float Roughness = 0.5f;
	};
}