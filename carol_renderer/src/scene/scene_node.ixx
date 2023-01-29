export module carol.renderer.scene.scene_node;
import carol.renderer.scene.mesh;
import <DirectXMath.h>;
import <string>;
import <memory>;
import <vector>;

namespace Carol {
    using std::unique_ptr;
    using std::vector;
    using std::wstring;
    using namespace DirectX;

    export class SceneNode
	{
	public:
		SceneNode()
        {
            XMStoreFloat4x4(&Transformation, XMMatrixIdentity());
        }

		wstring Name;
		vector<Mesh*> Meshes;
		vector<unique_ptr<SceneNode>> Children;
		XMFLOAT4X4 Transformation;
	};
}