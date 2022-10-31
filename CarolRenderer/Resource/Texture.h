#pragma once
#include "../DirectX/Buffer.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>
#include <string>

using Microsoft::WRL::ComPtr;
using std::unique_ptr;
using std::wstring;

namespace Carol
{
	class Device;
	class GraphicsCommandList;

	class Texture
	{
	public:
		DefaultBuffer* GetBuffer();
		void SetDesc();
		D3D12_SHADER_RESOURCE_VIEW_DESC GetDesc();
		void LoadTexture(wstring fileName, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, bool isSrgb = false);
	private:
		unique_ptr<DefaultBuffer> mTexture;
		D3D12_SHADER_RESOURCE_VIEW_DESC mTexDesc;
	};
}


