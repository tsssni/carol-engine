#pragma once
#include <scene/mesh.h>
#include <global.h>
#include <utils/d3dx12.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <DirectXPackedVector.h>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <unordered_map>
#include <span>

namespace Carol
{
	class AnimationClip;
	class TextureManager;
	class Timer;

	class SkinnedConstants
	{
	public:
		DirectX::XMFLOAT4X4 FinalTransforms[256];
		DirectX::XMFLOAT4X4 HistFinalTransforms[256];
	};

	class Model
	{
	public:
		Model();
		virtual ~Model();
		
		bool IsSkinned();
		void LoadGround();
		void LoadSkyBox();
		
		void ReleaseIntermediateBuffers();

		Mesh* GetMesh(std::wstring_view meshName);
		const std::unordered_map<std::wstring, std::unique_ptr<Mesh>>& GetMeshes();

		std::vector<std::wstring_view> GetAnimationClips();
		void SetAnimationClip(std::wstring_view clipName);

		SkinnedConstants* GetSkinnedConstants();
		void SetSkinnedCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr);

		void Update(Timer* timer);
		void GetSkinnedVertices(std::wstring_view clipName, const std::vector<Vertex>& vertices, std::vector<std::vector<Vertex>>& skinnedVertices);

	protected:
		std::wstring mModelName;
		std::wstring mTexDir;
		std::unordered_map<std::wstring, std::unique_ptr<Mesh>> mMeshes;

		bool mSkinned;
		float mTimePos;
		std::wstring mClipName;

		std::vector<int> mBoneHierarchy;
		std::vector<DirectX::XMFLOAT4X4> mBoneOffsets;

		std::unordered_map<std::wstring, std::unique_ptr<AnimationClip>> mAnimationClips;
		std::unordered_map<std::wstring, std::vector<std::vector<DirectX::XMFLOAT4X4>>> mFinalTransforms;
		std::unique_ptr<SkinnedConstants> mSkinnedConstants;
	};
}
