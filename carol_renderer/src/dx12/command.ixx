export module carol.renderer.dx12.command;
import carol.renderer.utils;
import <wrl/client.h>;
import <comdef.h>;

namespace Carol
{
    using Microsoft::WRL::ComPtr;

    export ComPtr<ID3D12Device> gDevice;
    export ComPtr<ID3D12GraphicsCommandList> gCommandList;
    export ComPtr<ID3D12CommandQueue> gCommandQueue;
    export ComPtr<ID3D12CommandSignature> gCommandSignature;

    export uint32_t gNumFrame;
    export uint32_t gCurrFrame;
}