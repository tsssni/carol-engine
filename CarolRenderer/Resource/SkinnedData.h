#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <string.h>

using std::wstring;

namespace Carol
{
	struct TranslationKeyframe
	{
		float TimePos;
		DirectX::XMFLOAT3 Translation;
	};

	struct ScaleKeyframe
	{
		float TimePos;
		DirectX::XMFLOAT3 Scale;
	};

	struct RotationQuatKeyframe
	{
		float TimePos;
		DirectX::XMFLOAT4 RotationQuat;
	};

	struct BoneAnimation
	{
		float GetStartTime()const;
		float GetEndTime()const;

		DirectX::XMVECTOR InterpolateTranslation(float t) const;
	    DirectX::XMVECTOR InterpolateScale(float t) const;
		DirectX::XMVECTOR InterpolateQuat(float t) const;
		void Interpolate(float t, DirectX::XMFLOAT4X4& M)const;

		std::vector<TranslationKeyframe> TranslationKeyframes;
		std::vector<ScaleKeyframe> ScaleKeyframes;
		std::vector<RotationQuatKeyframe> RotationQuatKeyframes;
	};

	struct AnimationClip
	{
		float GetClipStartTime()const;
		float GetClipEndTime()const;

		void Interpolate(float t, std::vector<DirectX::XMFLOAT4X4>& boneTransforms)const;

		std::vector<BoneAnimation> BoneAnimations; 	
	};

	class SkinnedData
	{
	public:

		UINT BoneCount()const;

		float GetClipStartTime(const wstring& clipName)const;
		float GetClipEndTime(const wstring& clipName)const;

		void Set(
			std::vector<int>& boneHierarchy, 
			std::vector<DirectX::XMFLOAT4X4>& boneOffsets,
			std::unordered_map<wstring, AnimationClip>& animations);

		void GetFinalTransforms(const wstring& clipName, float timePos, 
			 std::vector<DirectX::XMFLOAT4X4>& finalTransforms)const;

	private:
		std::vector<int> mBoneHierarchy;
		std::vector<DirectX::XMFLOAT4X4> mBoneOffsets;
		std::unordered_map<wstring, AnimationClip> mAnimations;
	};
}
