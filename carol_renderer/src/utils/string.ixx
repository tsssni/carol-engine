export module carol.renderer.utils.string;
import <string>;
import <string_view>;
import <comdef.h>;

namespace Carol{
    using std::string;
    using std::wstring;
    using std::string_view;
    using std::wstring_view;

    export wstring StringToWString(string_view str)
    {
        WCHAR buffer[512];
        MultiByteToWideChar(CP_ACP, 0, str.data(), -1, buffer, 512);
        return wstring(buffer);
    }

    export string WStringToString(wstring_view wstr)
    {
        CHAR buffer[512];
        WideCharToMultiByte(CP_ACP, 0, wstr.data(), -1, buffer, 512, NULL, NULL);
        return string(buffer);
    }
}