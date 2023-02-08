export module carol.renderer.dx12.command;
import carol.renderer.utils;
import <wrl/client.h>;
import <comdef.h>;

namespace Carol
{
    using Microsoft::WRL::ComPtr;

    export uint32_t gNumFrame;
    export uint32_t gCurrFrame;
}