#pragma once
#include <render_pass/render_pass.h>

namespace Carol
{
	class StructuredBuffer;
	class RawBuffer;
	class ComputePSO;

	class OitppllNode
	{
	public:
		DirectX::XMFLOAT4 Color;
		uint32_t Depth;
		uint32_t Next;
	};

	class OitppllPass : public RenderPass
	{
	public:
		OitppllPass();

		virtual void Draw()override;
		void SetIndirectCommandBuffer(MeshType type, const StructuredBuffer* indirectCommandBuffer);

		uint32_t GetPpllUavIdx()const;
		uint32_t GetStartOffsetUavIdx()const;
		uint32_t GetCounterUavIdx()const;
		uint32_t GetPpllSrvIdx()const;
		uint32_t GetStartOffsetSrvIdx()const;

	protected:
		virtual void InitPSOs()override;
		virtual void InitBuffers()override;

		std::unique_ptr<StructuredBuffer> mPpllBuffer;
		std::unique_ptr<RawBuffer> mStartOffsetBuffer; 
		std::unique_ptr<RawBuffer> mCounterBuffer;
		std::vector<const StructuredBuffer*> mIndirectCommandBuffer;
		ColorBuffer* mFrameMap;

		std::unique_ptr<MeshPSO> mBuildOitppllStaticMeshPSO;
		std::unique_ptr<MeshPSO> mBuildOitppllSkinnedMeshPSO;
		std::unique_ptr<ComputePSO> mOitppllComputePSO;
	};
}
