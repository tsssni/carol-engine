#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <vector>
#include <string>
#include <memory>
#include <wrl/client.h>

namespace Carol
{
	class Shader
	{
	public:
		Shader(
			const std::wstring& fileName,
			const std::vector<std::wstring>& defines,
			const std::wstring& entryPoint,
			const std::wstring& target);

		LPVOID GetBufferPointer();
		size_t GetBufferSize();

		static void InitCompiler();
	private:
		std::vector<LPCWSTR> SetArgs(const std::vector<std::wstring>& defines, const std::wstring& entryPoint, const std::wstring& target);
		Microsoft::WRL::ComPtr<IDxcResult> Compile(const std::wstring& fileName, std::vector<LPCWSTR>& args);
		void CheckError(Microsoft::WRL::ComPtr<IDxcResult>& result);
		void OutputPdb(Microsoft::WRL::ComPtr<IDxcResult>& result);

		static Microsoft::WRL::ComPtr<IDxcCompiler3> Compiler;
		static Microsoft::WRL::ComPtr<IDxcUtils> Utils;
		static Microsoft::WRL::ComPtr<IDxcIncludeHandler> IncludeHandler;
		Microsoft::WRL::ComPtr<IDxcBlob> mShader;
	};
}