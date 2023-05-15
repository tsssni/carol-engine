#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <vector>
#include <string>
#include <string_view>
#include <span>
#include <memory>
#include <unordered_map>
#include <wrl/client.h>

namespace Carol
{
	class Shader
	{
	public:
		Shader(std::string_view path);
		void* GetBufferPointer()const;
		size_t GetBufferSize()const;
	private:
		std::vector<std::byte> mBlob;
	};

	class ShaderManager
	{
	public:
		ShaderManager();
		Shader* LoadShader(std::string_view path);

	private:
		std::unordered_map<std::string, std::unique_ptr<Shader>> mShaders;
	};
}