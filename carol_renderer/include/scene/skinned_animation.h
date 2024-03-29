#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

namespace Carol
{
	class TranslationKeyframe
	{
	public:
		float TimePos;
		DirectX::XMFLOAT3 Translation;
	};

	class ScaleKeyframe
	{
	public:
		float TimePos;
		DirectX::XMFLOAT3 Scale;
	};

	class RotationQuatKeyframe
	{
	public:
		float TimePos;
		DirectX::XMFLOAT4 RotationQuat;
	};

	class BoneAnimation
	{
	public:
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

	class AnimationClip
	{
	public:
		void CalcClipStartTime();
		void CalcClipEndTime();
		float GetClipStartTime();
		float GetClipEndTime();

		void Interpolate(float t, std::vector<DirectX::XMFLOAT4X4>& boneTransforms)const;
		std::vector<BoneAnimation> BoneAnimations; 	

	private:
		float mStartTime = -1.f;
		float mEndTime = -1.f;
	};
}
