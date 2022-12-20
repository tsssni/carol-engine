#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>
#include <string>

namespace Carol
{
	class GlobalResources;
	class DefaultResource;

	class Texture
	{
	public:
		DefaultResource* GetBuffer();
		void SetDesc();
		D3D12_SHADER_RESOURCE_VIEW_DESC GetDesc();
		void LoadTexture(GlobalResources* globalResources, std::wstring fileName, bool isSrgb = false);
		void ReleaseIntermediateBuffer();
	private:
		std::unique_ptr<DefaultResource> mTexture;
		D3D12_SHADER_RESOURCE_VIEW_DESC mTexDesc;

		bool mIsCube = false;
		bool mIsVolume = false;
	};
}


