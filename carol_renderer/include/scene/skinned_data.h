#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <string.h>
#include <vector>
#include <memory>
#include <unordered_map>

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
		void GetFrames(std::vector<DirectX::XMFLOAT4X4>& M)const;

		std::vector<TranslationKeyframe> TranslationKeyframes;
		std::vector<ScaleKeyframe> ScaleKeyframes;
		std::vector<RotationQuatKeyframe> RotationQuatKeyframes;
	};

	class AnimationClip
	{
	public:
		float GetClipStartTime()const;
		float GetClipEndTime()const;

		void Interpolate(float t, std::vector<DirectX::XMFLOAT4X4>& boneTransforms)const;
		void GetFrames(std::vector<std::vector<DirectX::XMFLOAT4X4>>& frames)const;
		std::vector<BoneAnimation> BoneAnimations; 	
	};

	class SkinnedData
	{
	public:
		DirectX::XMFLOAT4X4 FinalTransforms[256];
		DirectX::XMFLOAT4X4 HistoryFinalTransforms[256];
	};
}
