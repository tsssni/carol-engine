#pragma once
#include <string>
#include <string_view>
#include <Windows.h>

namespace Carol
{
    class DxException
    {
    public:
        DxException() = default;
        DxException(HRESULT hr, std::string_view functionName, std::string_view filename, int lineNumber);
        std::string ToString()const;
    
    private:
        HRESULT ErrorCode = S_OK;
        std::string FunctionName;
        std::string Filename;
        int LineNumber = -1;
    };

    #ifndef ThrowIfFailed
    #define ThrowIfFailed(x)                                              \
    {                                                                     \
        HRESULT hr__ = (x);                                               \
        std::string wfn = __FILE__;                                       \
        if(FAILED(hr__)) { throw Carol::DxException(hr__, #x, wfn, __LINE__); }  \
    }
    #endif
}


