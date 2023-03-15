#pragma once
#include <scene/mesh.h>
#include <global.h>
#include <utils/d3dx12.h>
#include <scene/light.h>
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
	class Light;

	class SkinnedConstants
	{
	public:
		DirectX::XMFLOAT4X4 FinalTransforms[256] = {};
		DirectX::XMFLOAT4X4 HistFinalTransforms[256] = {};
	};

	class Model
	{
	public:
		Model(
			TextureManager* textureManager
		);
		virtual ~Model();
		
		bool IsSkinned()const;
		void LoadSkyBox(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmdList,
			Heap* defaultBuffersHeap,
			Heap* uploadBuffersHeap,
			DescriptorManager* descriptorManager);
		
		void ReleaseIntermediateBuffers();

		const Mesh* GetMesh(std::wstring_view meshName)const;
		const std::unordered_map<std::wstring, std::unique_ptr<Mesh>>& GetMeshes()const;

		std::vector<std::wstring_view> GetAnimationClips()const;
		void SetAnimationClip(std::wstring_view clipName);

		const SkinnedConstants* GetSkinnedConstants()const;
		void SetMeshCBAddress(std::wstring_view meshName, D3D12_GPU_VIRTUAL_ADDRESS addr);
		void SetSkinnedCBAddress(D3D12_GPU_VIRTUAL_ADDRESS addr);

		void Update(Timer* timer);
		void GetSkinnedVertices(std::wstring_view clipName, const std::vector<Vertex>& vertices, std::vector<std::vector<Vertex>>& skinnedVertices)const;

		std::span<std::unique_ptr<Light>> GetLights(LightType type);

	protected:
		std::wstring mModelName;
		std::wstring mTexDir;
		std::unordered_map<std::wstring, std::unique_ptr<Mesh>> mMeshes;

		bool mSkinned = false;
		float mTimePos = 0.f;
		std::wstring mClipName;

		std::vector<int> mBoneHierarchy;
		std::vector<DirectX::XMFLOAT4X4> mBoneOffsets;

		std::unordered_map<std::wstring, std::unique_ptr<AnimationClip>> mAnimationClips;
		std::unordered_map<std::wstring, std::vector<std::vector<DirectX::XMFLOAT4X4>>> mFinalTransforms;
		std::unique_ptr<SkinnedConstants> mSkinnedConstants;

		TextureManager* mTextureManager;
		std::vector<std::wstring> mTexturePath;

		std::vector<std::unique_ptr<Light>> mLights[LIGHT_TYPE_COUNT];
	};
}
