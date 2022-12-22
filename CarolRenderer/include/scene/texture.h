#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>
#include <string>

namespace Carol
{
	class DefaultResource;
	class Heap;

	class Texture
	{
	public:
		DefaultResource* GetResource();
		D3D12_SHADER_RESOURCE_VIEW_DESC GetDesc();
		void LoadTexture(ID3D12GraphicsCommandList* cmdList, Heap* texHeap, Heap* uploadHeap, std::wstring fileName, bool isSrgb = false);
		void ReleaseIntermediateBuffer();
	private:
		void SetDesc();
		std::unique_ptr<DefaultResource> mTexture;
		D3D12_SHADER_RESOURCE_VIEW_DESC mTexDesc;

		bool mIsCube = false;
		bool mIsVolume = false;
	};
}


