#pragma once
#include <scene/mesh.h>
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
	class Mesh;

	class SkinnedConstants
	{
	public:
		DirectX::XMFLOAT4X4 FinalTransforms[256] = {};
		DirectX::XMFLOAT4X4 HistFinalTransforms[256] = {};
	};

	class Model
	{
	public:
		Model();
		virtual ~Model();
		
		bool IsSkinned()const;
		
		void ReleaseIntermediateBuffers();

		const Mesh* GetMesh(std::string_view meshName)const;
		const std::unordered_map<std::string, std::unique_ptr<Mesh>>& GetMeshes()const;

		std::vector<std::string_view> GetAnimationClips()const;
		void SetAnimationClip(std::string_view clipName);

		const SkinnedConstants* GetSkinnedConstants()const;
		void SetMeshCBAddress(std::string_view meshName, D3D12_GPU_VIRTUAL_ADDRESS addr);
		void SetSkinnedCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr);

		void Update(Timer* timer);
		void GetFinalTransforms(std::string_view clipName, float t, std::vector<DirectX::XMFLOAT4X4>& toRootTransforms);
		void GetSkinnedVertices(std::string_view clipName, std::span<Vertex> vertices, std::vector<std::vector<Vertex>>& skinnedVertices)const;

	protected:
		std::string mModelName;
		std::string mTexDir;
		std::unordered_map<std::string, std::unique_ptr<Mesh>> mMeshes;

		bool mSkinned = false;
		float mTimePos = 0.f;
		std::string mClipName;

		std::vector<int> mBoneHierarchy;
		std::vector<DirectX::XMFLOAT4X4> mBoneOffsets;

		std::unordered_map<std::string, std::unique_ptr<AnimationClip>> mAnimationClips;
		std::unordered_map<std::string, std::vector<std::vector<DirectX::XMFLOAT4X4>>> mFrameTransforms;
		std::unique_ptr<SkinnedConstants> mSkinnedConstants;

		std::vector<std::string> mTexturePath;
	};
}
