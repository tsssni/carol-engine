#pragma once
#include <string>
#include <comdef.h>

using std::string;
using std::wstring;

namespace Carol
{
    inline wstring StringToWString(const string& str)
    {
        WCHAR buffer[512];
        MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
        return wstring(buffer);
    }

    inline string WStringToString(const wstring& wstr)
    {
        CHAR buffer[512];
        WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, buffer, 512, NULL, NULL);
        return string(buffer);
    }

    class DxException
    {
    public:
        DxException() = default;
        DxException(HRESULT hr, const wstring& functionName, const wstring& filename, int lineNumber) :
            ErrorCode(hr),
            FunctionName(functionName),
            Filename(filename),
            LineNumber(lineNumber)
        {}

        wstring ToString()const
        {
            _com_error err(ErrorCode);
            wstring msg = err.ErrorMessage();

            return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
        }

        HRESULT ErrorCode = S_OK;
        wstring FunctionName;
        wstring Filename;
        int LineNumber = -1;
    };

    #ifndef ThrowIfFailed
    #define ThrowIfFailed(x)                                              \
    {                                                                     \
        HRESULT hr__ = (x);                                               \
        wstring wfn = StringToWString(__FILE__);                       \
        if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
    }
    #endif

    template<typename T>
    requires(!std::is_lvalue_reference_v<T>)
    inline T* GetRvaluePtr(T&& v) {
        return &v;
    }
}


