#pragma once
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <memory>


namespace Carol {

	class Mesh;
	
	class SceneNode
	{
	public:
		SceneNode();
		std::wstring Name;
		std::vector<Mesh*> Meshes;
		std::vector<std::unique_ptr<SceneNode>> Children;
		DirectX::XMFLOAT4X4 Transformation;
	};
}


