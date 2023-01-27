#pragma once
#include <string>
#include <string_view>
#include <comdef.h>

namespace Carol
{
    inline std::wstring StringToWString(std::string_view str)
    {
        WCHAR buffer[512];
        MultiByteToWideChar(CP_ACP, 0, str.data(), -1, buffer, 512);
        return std::wstring(buffer);
    }

    inline std::string WStringToString(std::wstring_view wstr)
    {
        CHAR buffer[512];
        WideCharToMultiByte(CP_ACP, 0, wstr.data(), -1, buffer, 512, NULL, NULL);
        return std::string(buffer);
    }

    class DxException
    {
    public:
        DxException() = default;
        DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber) :
            ErrorCode(hr),
            FunctionName(functionName),
            Filename(filename),
            LineNumber(lineNumber)
        {}

        std::wstring ToString()const
        {
            _com_error err(ErrorCode);
            std::wstring msg = err.ErrorMessage();

            return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
        }

        HRESULT ErrorCode = S_OK;
        std::wstring FunctionName;
        std::wstring Filename;
        int LineNumber = -1;
    };

    #ifndef ThrowIfFailed
    #define ThrowIfFailed(x)                                              \
    {                                                                     \
        HRESULT hr__ = (x);                                               \
        std::wstring wfn = StringToWString(__FILE__);                       \
        if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
    }
    #endif

    template<typename T>
    requires(!std::is_lvalue_reference_v<T>)
    inline T* GetRvaluePtr(T&& v) {
        return &v;
    }
}


