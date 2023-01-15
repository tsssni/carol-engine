#include <scene/skinned_data.h>
#include <cmath>

namespace Carol {
	using std::vector;
	using std::make_unique;
	using namespace DirectX;
}

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

Carol::XMVECTOR Carol::BoneAnimation::InterpolateTranslation(float t) const
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

Carol::XMVECTOR Carol::BoneAnimation::InterpolateScale(float t) const
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

Carol::XMVECTOR Carol::BoneAnimation::InterpolateQuat(float t) const
{
	if (RotationQuatKeyframes.size() == 0)
	{
		return { 0.0f,0.0f,0.0f,1.0f };
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

void Carol::BoneAnimation::GetFrames(std::vector<DirectX::XMFLOAT4X4>& M)const
{
	int size = ScaleKeyframes.size();
	M.resize(size);

	for (int i = 0; i < size; ++i)
	{
		XMVECTOR S = XMLoadFloat3(&ScaleKeyframes[i].Scale);
		XMVECTOR Q = XMLoadFloat4(&RotationQuatKeyframes[i].RotationQuat);
		XMVECTOR T = XMLoadFloat3(&TranslationKeyframes[i].Translation);
		XMVECTOR origin = { 0.0f,0.0f,0.0f,1.0f };
		XMStoreFloat4x4(&M[i], XMMatrixAffineTransformation(S, origin, Q, T));
	}
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

void Carol::AnimationClip::Interpolate(float t, vector<DirectX::XMFLOAT4X4>& boneTransforms) const
{
	for (int i = 0; i < boneTransforms.size(); ++i)
	{	
		BoneAnimations[i].Interpolate(t, boneTransforms[i]);
	}
}

void Carol::AnimationClip::GetFrames(std::vector<std::vector<DirectX::XMFLOAT4X4>>& frames) const
{
	int size = BoneAnimations.size();
	int maxFrame = 0;
	vector<vector<XMFLOAT4X4>> M(size);

	for (int i = 0; i < size; ++i)
	{
		BoneAnimations[i].GetFrames(M[i]);
		if (M[i].size() > maxFrame)
		{
			maxFrame = M[i].size();
		}
	}
	
	frames.resize(maxFrame);
	XMFLOAT4X4 identity;
	XMStoreFloat4x4(&identity, XMMatrixIdentity());

	for (int i = 0; i < maxFrame; ++i)
	{
		frames[i].resize(size);

		for (int j = 0; j < size; ++j)
		{
			if (i >= M[j].size())
			{
				frames[i][j] = identity;
			}
			else
			{
				frames[i][j] = M[j][i];
			}
		}
	}
}
