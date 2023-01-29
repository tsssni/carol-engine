export module carol.renderer.utils.exception;
import carol.renderer.utils.string;
import <string>;
import <comdef.h>;

namespace Carol
{
    using std::wstring;
    
    export class DxException
    {
    public:
        DxException() = default;
        DxException(HRESULT hr) :
            ErrorCode(hr)
        {}

        wstring ToString()const
        {
            _com_error err(ErrorCode);
            return StringToWString(err.ErrorMessage());
        }

        HRESULT ErrorCode = S_OK;
    };

    export void ThrowIfFailed(HRESULT hr)
    {                    
        if(FAILED(hr)) 
        { 
            throw DxException(hr);
        }
    }
}