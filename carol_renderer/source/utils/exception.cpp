#include <utils/exception.h>
#include <comdef.h>
#include <system_error>

Carol::DxException::DxException(HRESULT hr, std::string_view functionName, std::string_view filename, int lineNumber)
	:ErrorCode(hr),
	FunctionName(functionName),
	Filename(filename),
	LineNumber(lineNumber)
{}

std::string Carol::DxException::ToString()const
{
	_com_error err(ErrorCode);
	return FunctionName + " failed in " + Filename + "; line " + std::to_string(LineNumber) + "; error: " + err.ErrorMessage();
}

