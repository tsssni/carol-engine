#pragma once
#include <string>
#include <string_view>

namespace Carol
{
	std::string WStringToString(std::wstring_view wstr);
	std::wstring StringToWString(std::string_view str);
}
