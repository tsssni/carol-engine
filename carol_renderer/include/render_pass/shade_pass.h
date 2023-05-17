#pragma once
#include <render_pass/render_pass.h>
#include <scene/light.h>
#include <scene/mesh.h>
#include <DirectXMath.h>
#include <memory>
#include <vector>

#define MAX_LIGHTS 16

namespace Carol
{
	class ColorBuffer;
	class StructuredBuffer;
	class StructuredBufferPool;
	class MeshPSO;
	class ComputePSO;

	class ShadePass :public RenderPass
	{
	public:
		ShadePass();

		virtual void Draw();

	protected:
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;
		
		std::unique_ptr<ComputePSO> mShadeComputePSO;
	};
}