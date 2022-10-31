#pragma once
#include "Buffer.h"
#include <d3d12.h>
#include <vector>
#include <string>
#include <memory>
#include <wrl/client.h>

using std::vector;
using std::string;
using std::wstring;
using std::unique_ptr;
using Microsoft::WRL::ComPtr;

namespace Carol
{
	class Shader
	{
	public:
		Shader() = default;

		Blob* GetBlob();
		void CompileShader(
			const wstring& filenName,
			const D3D_SHADER_MACRO* defines,
			const string& entryPoint,
			const string& target);
	private:
		unique_ptr<Blob> mShader;
	};
}