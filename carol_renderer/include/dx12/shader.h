#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <vector>
#include <string>
#include <string_view>
#include <span>
#include <memory>
#include <wrl/client.h>

namespace Carol
{
	class Shader
	{
	public:
		Shader(
			std::wstring_view fileName,
			std::span<const std::wstring_view> defines,
			std::wstring_view entryPoint,
			std::wstring_view target);

		LPVOID GetBufferPointer()const;
		size_t GetBufferSize()const;

		static void InitCompiler();
	private:
		std::vector<LPCWSTR> SetArgs(std::span<const std::wstring_view> defines, std::wstring_view entryPoint, std::wstring_view target);
		Microsoft::WRL::ComPtr<IDxcResult> Compile(std::wstring_view fileName, std::span<LPCWSTR> args);
		void CheckError(Microsoft::WRL::ComPtr<IDxcResult>& result);
		void OutputPdb(Microsoft::WRL::ComPtr<IDxcResult>& result);

		static Microsoft::WRL::ComPtr<IDxcCompiler3> Compiler;
		static Microsoft::WRL::ComPtr<IDxcUtils> Utils;
		static Microsoft::WRL::ComPtr<IDxcIncludeHandler> IncludeHandler;
		Microsoft::WRL::ComPtr<IDxcBlob> mShader;
	};
}