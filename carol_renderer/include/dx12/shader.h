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
		void SetFileName(std::wstring_view fileName);
		void SetDefines(const std::vector<std::wstring_view>& defines);
		void SetEntryPoint(std::wstring_view entryPoint);
		void SetTarget(std::wstring_view target);
		void Finalize();

		LPVOID GetBufferPointer()const;
		size_t GetBufferSize()const;

		static void InitCompiler();
		static void InitShaders();
	private:
		std::vector<LPCWSTR> SetArgs();
		Microsoft::WRL::ComPtr<IDxcResult> Compile(std::span<LPCWSTR> args);
		void CheckError(Microsoft::WRL::ComPtr<IDxcResult>& result);
		void OutputPdb(Microsoft::WRL::ComPtr<IDxcResult>& result);

		std::wstring mFileName;
		std::vector<std::wstring> mDefines;
		std::wstring mEntryPoint;
		std::wstring mTarget;

		Microsoft::WRL::ComPtr<IDxcBlob> mShader;
	};
}