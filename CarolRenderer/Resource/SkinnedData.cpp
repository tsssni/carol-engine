#include "SkinnedData.h"
#include <cmath>

using namespace DirectX;

float Carol::BoneAnimation::GetStartTime() const
{
	float tTrans = D3D12_FLOAT32_MAX;
	float tScale = D3D12_FLOAT32_MAX;
	float tQuat = D3D12_FLOAT32_MAX;

	if(TranslationKeyframes.size()!=0) tTrans = TranslationKeyframes.front().TimePos;
	if(ScaleKeyframes.size()!=0) tScale = ScaleKeyframes.front().TimePos;
	if(RotationQuatKeyframes.size()!=0) tQuat = RotationQuatKeyframes.front().TimePos;
	return std::min(std::min(tTrans, tScale), tQuat);
}

float Carol::BoneAnimation::GetEndTime() const
{
	float tTrans = 0.0f;
	float tScale = 0.0f;
	float tQuat = 0.0f;

	if(TranslationKeyframes.size()!=0) tTrans = TranslationKeyframes.back().TimePos;
	if(ScaleKeyframes.size()!=0) tScale = ScaleKeyframes.back().TimePos;
	if(RotationQuatKeyframes.size()!=0) tQuat = RotationQuatKeyframes.back().TimePos;
	return std::max(std::max(tTrans, tScale), tQuat);
}

XMVECTOR Carol::BoneAnimation::InterpolateTranslation(float t) const
{
	if (TranslationKeyframes.size() == 0)
	{
		return { 0.0f,0.0f,0.0f };
	}

	if (t < TranslationKeyframes.front().TimePos)
	{
		return XMLoadFloat3(&TranslationKeyframes.front().Translation);
	}
	else if (t > TranslationKeyframes.back().TimePos)
	{
		return XMLoadFloat3(&TranslationKeyframes.back().Translation);
	}
	else
	{
		for (int i = 0; i < TranslationKeyframes.size() - 1; ++i)
		{
			if (TranslationKeyframes[i].TimePos <= t && TranslationKeyframes[i + 1].TimePos > t)
			{
				float lerpFactor = (t - TranslationKeyframes[i].TimePos) / (TranslationKeyframes[i + 1].TimePos - TranslationKeyframes[i].TimePos);

				XMVECTOR preT = XMLoadFloat3(&TranslationKeyframes[i].Translation);
				XMVECTOR sucT = XMLoadFloat3(&TranslationKeyframes[i + 1].Translation);
				return XMVectorLerp(preT, sucT, lerpFactor);
			}
		}
	}
}

XMVECTOR Carol::BoneAnimation::InterpolateScale(float t) const
{
	if (ScaleKeyframes.size() == 0)
	{
		return { 1.0f,1.0f,1.0f };
	}

	if (t < ScaleKeyframes.front().TimePos)
	{
		return XMLoadFloat3(&ScaleKeyframes.front().Scale);
	}
	else if (t >= ScaleKeyframes.back().TimePos)
	{
		return XMLoadFloat3(&ScaleKeyframes.back().Scale);
	}
	else
	{
		for (int i = 0; i < ScaleKeyframes.size() - 1; ++i)
		{
			if (ScaleKeyframes[i].TimePos <= t && ScaleKeyframes[i + 1].TimePos > t)
			{
				float lerpFactor = (t - ScaleKeyframes[i].TimePos) / (ScaleKeyframes[i + 1].TimePos - ScaleKeyframes[i].TimePos);

				XMVECTOR preS = XMLoadFloat3(&ScaleKeyframes[i].Scale);
				XMVECTOR sucS = XMLoadFloat3(&ScaleKeyframes[i + 1].Scale);
				return XMVectorLerp(preS, sucS, lerpFactor);
			}
		}
	}
}

XMVECTOR Carol::BoneAnimation::InterpolateQuat(float t) const
{
	if (RotationQuatKeyframes.size() == 0)
	{
		return { 1.0f,0.0f,0.0f,0.0f };
	}

	if (t < RotationQuatKeyframes.front().TimePos)
	{
		return XMLoadFloat4(&RotationQuatKeyframes.front().RotationQuat);
	}
	else if (t > RotationQuatKeyframes.back().TimePos)
	{
		return XMLoadFloat4(&RotationQuatKeyframes.back().RotationQuat);
	}
	else
	{
		for (int i = 0; i < RotationQuatKeyframes.size() - 1; ++i)
		{
			if (RotationQuatKeyframes[i].TimePos <= t && RotationQuatKeyframes[i + 1].TimePos > t)
			{
				float lerpFactor = (t - RotationQuatKeyframes[i].TimePos) / (RotationQuatKeyframes[i + 1].TimePos - RotationQuatKeyframes[i].TimePos);

				XMVECTOR preQ = XMLoadFloat4(&RotationQuatKeyframes[i].RotationQuat);
				XMVECTOR sucQ = XMLoadFloat4(&RotationQuatKeyframes[i + 1].RotationQuat);
				return XMQuaternionSlerp(preQ, sucQ, lerpFactor);
			}
		}
	}

}

void Carol::BoneAnimation::Interpolate(float t, DirectX::XMFLOAT4X4& M) const
{
	XMVECTOR S = InterpolateScale(t);
	XMVECTOR Q = InterpolateQuat(t);
	XMVECTOR T = InterpolateTranslation(t);
	XMVECTOR origin = { 0.0f,0.0f,0.0f,1.0f };
	XMStoreFloat4x4(&M, XMMatrixAffineTransformation(S, origin, Q, T));
}

float Carol::AnimationClip::GetClipStartTime() const
{
	float startTime = UINT32_MAX;
	for (auto anime : BoneAnimations)
	{
		startTime = std::min(startTime, anime.GetStartTime());
	}
	return startTime;
}

float Carol::AnimationClip::GetClipEndTime() const
{
	float endTime = 0;
	for (auto anime : BoneAnimations)
	{
		endTime = std::max(endTime, anime.GetEndTime());
	}
	return endTime;
}

void Carol::AnimationClip::Interpolate(float t, std::vector<DirectX::XMFLOAT4X4>& boneTransforms) const
{
	for (int i = 0; i < boneTransforms.size(); ++i)
	{
		BoneAnimations[i].Interpolate(t, boneTransforms[i]);
	}
}

uint32_t Carol::SkinnedData::BoneCount() const
{
	return mBoneHierarchy.size();
}

float Carol::SkinnedData::GetClipStartTime(const wstring& clipName) const
{
	return mAnimations.at(clipName).GetClipStartTime();
}

float Carol::SkinnedData::GetClipEndTime(const wstring& clipName) const
{
	return mAnimations.at(clipName).GetClipEndTime();
}

void Carol::SkinnedData::Set(std::vector<int>& boneHierarchy, std::vector<DirectX::XMFLOAT4X4>& boneOffsets, std::unordered_map<wstring, AnimationClip>& animations)
{
	mBoneHierarchy = std::move(boneHierarchy);
	mBoneOffsets = std::move(boneOffsets);
	mAnimations = std::move(animations);
}

void Carol::SkinnedData::GetFinalTransforms(const wstring& clipName, float timePos, std::vector<DirectX::XMFLOAT4X4>& finalTransforms) const
{
	uint32_t numBones = mBoneOffsets.size();

	std::vector<XMFLOAT4X4> toParentTransforms(numBones);

	auto clip = mAnimations.find(clipName);
	clip->second.Interpolate(timePos, toParentTransforms);

	std::vector<XMFLOAT4X4> toRootTransforms(numBones);
	toRootTransforms[0] = toParentTransforms[0];

	for (uint32_t i = 1; i < numBones; ++i)
	{
		XMMATRIX toParent = XMLoadFloat4x4(&toParentTransforms[i]);

		int parentIndex = mBoneHierarchy[i];
		if (parentIndex == -1)
		{
			continue;
		}
		XMMATRIX parentToRoot = XMLoadFloat4x4(&toRootTransforms[parentIndex]);

		XMMATRIX toRoot = XMMatrixMultiply(toParent, parentToRoot);

		XMStoreFloat4x4(&toRootTransforms[i], toRoot);
	}

	for (UINT i = 0; i < numBones; ++i)
	{
		XMMATRIX offset = XMLoadFloat4x4(&mBoneOffsets[i]);
		XMMATRIX toRoot = XMLoadFloat4x4(&toRootTransforms[i]);
		XMMATRIX finalTransform = XMMatrixMultiply(offset, toRoot);
		XMStoreFloat4x4(&finalTransforms[i], XMMatrixTranspose(finalTransform));
	}
}
