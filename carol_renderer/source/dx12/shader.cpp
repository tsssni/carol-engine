#include <dx12/shader.h>
#include <utils/exception.h>
#include <global.h>
#include <fstream>

Carol::Shader::Shader(std::string_view path)
{
    std::ifstream file(path.data(), std::ios::ate | std::ios::binary);
    size_t size = file.tellg();
    mBlob.resize(size);

    file.seekg(0);
    file.read((char*)mBlob.data(), size);
    file.close();
}

void* Carol::Shader::GetBufferPointer()const
{
    return (void*)mBlob.data();
}

size_t Carol::Shader::GetBufferSize()const
{
    return mBlob.size();
}


Carol::ShaderManager::ShaderManager()
{
}   

Carol::Shader* Carol::ShaderManager::LoadShader(std::string_view path)
{
    std::string pathStr = path.data();

    if (mShaders.count(pathStr) == 0)
    {
        mShaders[pathStr] = std::make_unique<Shader>(path);
    }

    return mShaders[pathStr].get();
}