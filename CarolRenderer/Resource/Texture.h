#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>
#include <string>

using Microsoft::WRL::ComPtr;
using std::unique_ptr;
using std::wstring;

namespace Carol
{
	class RenderData;
	class DefaultResource;

	class Texture
	{
	public:
		DefaultResource* GetBuffer();
		void SetDesc();
		D3D12_SHADER_RESOURCE_VIEW_DESC GetDesc();
		void LoadTexture(RenderData* renderData, wstring fileName, bool isSrgb = false);
		void ReleaseIntermediateBuffer();
	private:
		unique_ptr<DefaultResource> mTexture;
		D3D12_SHADER_RESOURCE_VIEW_DESC mTexDesc;

		bool mIsCube = false;
		bool mIsVolume = false;
	};
}


