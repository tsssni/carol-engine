#include <scene/scene_node.h>

namespace Carol
{
	using namespace DirectX;
}

Carol::SceneNode::SceneNode()
{
	XMStoreFloat4x4(&Transformation, XMMatrixIdentity());
}
