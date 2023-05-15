#include <utils/string.h>
#include <comdef.h>

std::string Carol::WStringToString(std::wstring_view wstr)
{
	CHAR buffer[512];
	WideCharToMultiByte(CP_ACP, 0, wstr.data(), -1, buffer, 512, NULL, NULL);
	return std::string(buffer);
}

std::wstring Carol::StringToWString(std::string_view str)
{
    WCHAR buffer[512];
	MultiByteToWideChar(CP_ACP, 0, str.data(), -1, buffer, 512);
	return std::wstring(buffer);
}
