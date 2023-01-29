export module carol.renderer.utils.rvalue;
import <comdef.h>;

namespace Carol
{
    export template<typename T>
    requires(!std::is_lvalue_reference_v<T>)
    T* GetRvaluePtr(T&& v) {
        return &v;
    }
}