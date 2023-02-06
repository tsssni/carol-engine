// MainWindow.cpp : 定义应用程序的入口点。
//

#include "win32/resource.h"
#include "win32/framework.h"
#include <renderer.h>
#include <dx12.h>
#include <render_pass.h>
#include <scene.h>
#include <utils.h>
#include <windowsx.h>
#include <ShlObj.h>
#include <DirectXMath.h>

#define MAX_LOADSTRING 100

using std::wstring;
using Microsoft::WRL::ComPtr;
using namespace DirectX;

// 全局变量:
HINSTANCE hInst;                                // 当前实例
HWND hWnd;                                      // 当前窗口
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名

Carol::Renderer* renderer;
wstring loadModelName;

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

LRESULT CALLBACK    LoadWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    AnimationWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    DeleteWndProc(HWND, UINT, WPARAM, LPARAM);


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: 在此处放置代码。

    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CAROLRENDERER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 执行应用程序初始化:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    MSG msg = {0};

    RECT clientRect;
    GetClientRect(hWnd, &clientRect);

    uint32_t width = clientRect.right - clientRect.left;
    uint32_t height = clientRect.bottom - clientRect.top;

    try
    {
        renderer = new Carol::Renderer(hWnd,width,height);
        renderer->LoadModel(L"C:\\Minotaur\\animation\\Minotaur@Attack.FBX", L"C:\\Minotaur\\texture", L"m", XMMatrixScaling(0.1f, 0.1f, 0.1f), true);
        renderer->SetAnimation(L"m", L"Take 001");

		// 主消息循环:
        while (msg.message != WM_QUIT)
		{
			if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				renderer->Tick();

                if (!renderer->IsPaused())
                {
					renderer->CalcFrameState();
                    renderer->Update();
					renderer->Draw();
                }
                else
                {
                    Sleep(100);
                }
				
			}
		}
        
        delete renderer;
        return (int) msg.wParam;

    }
    catch (Carol::DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    } 
}



//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CAROLRENDERER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDR_MENU);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目标: 保存实例句柄并创建主窗口
//
//   注释:
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // 将实例句柄存储在全局变量中

   hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
   
   RECT clientRect;
   GetClientRect(hWnd, &clientRect);

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
//  WM_COMMAND  - 处理应用程序菜单
//  WM_PAINT    - 绘制主窗口
//  WM_DESTROY  - 发送退出消息并返回
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static bool pushed = false;

    switch (message)
    {
    case WM_ACTIVATE:
        {
			if (renderer)
			{
				int fActive = LOWORD(wParam);
				if (fActive & (WA_ACTIVE | WA_CLICKACTIVE))
				{
					renderer->SetPaused(false);
				}
				else
				{
					renderer->SetPaused(true);
				}
			}
        }

        break;
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            
            // 分析菜单选择:
            switch (wmId)
            {
            case ID_LOAD_MODEL:
                renderer->SetPaused(true);
                renderer->Stop();
                DialogBox(hInst, MAKEINTRESOURCE(IDD_LOAD), hWnd, LoadWndProc);

                renderer->SetPaused(false);
                renderer->Start();
                break;
            case ID_DELETE_MODEL:
                renderer->SetPaused(true);
                renderer->Stop();
                DialogBox(hInst, MAKEINTRESOURCE(IDD_DELETE), hWnd, DeleteWndProc);

                renderer->SetPaused(false);
                renderer->Start();
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
        // WM_SIZE is sent when the user resizes the window.  
    case WM_SIZE:
        // Save the new client area dimensions.
        if (renderer)
        {
            uint32_t width = LOWORD(lParam);
			uint32_t height = HIWORD(lParam);

            if (wParam == SIZE_MINIMIZED)
            {
                renderer->SetPaused(true);
                renderer->Stop();
                renderer->SetMinimized(true);
                renderer->SetMaximized(false);
            }
            else if (wParam == SIZE_MAXIMIZED)
            {
                renderer->SetPaused(false);
                renderer->SetMinimized(false);
                renderer->SetMaximized(true);               
                renderer->OnResize(width, height);
            }
            else if (wParam == SIZE_RESTORED)
            {

                // Restoring from minimized state?
                if (renderer->IsIconic())
                {
                    renderer->SetPaused(false);
                    renderer->SetMinimized(false);
                    renderer->OnResize(width, height);
                }

                // Restoring from maximized state?
                else if (renderer->IsZoomed())
                {
					renderer->SetPaused(false);
                    renderer->SetMaximized(false);
                    renderer->OnResize(width, height);
                }
                else if (renderer->IsResizing())
                {
                    // If user is dragging the resize bars, we do not resize 
                    // the buffers here because as the user continuously 
                    // drags the resize bars, a stream of WM_SIZE messages are
                    // sent to the window, and it would be pointless (and slow)
                    // to resize for each WM_SIZE message received from dragging
                    // the resize bars.  So instead, we reset after the user is 
                    // done resizing the window and releases the resize bars, which 
                    // sends a WM_EXITSIZEMOVE message.
                }
                else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
                {
                    renderer->OnResize(width, height);
                }
            }
        }

        break;

        // WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
    case WM_ENTERSIZEMOVE:
        if (renderer)
        {
            renderer->SetPaused(true);
			renderer->SetResizing(true);
			renderer->Stop();
        }
        
        break;

        // WM_EXITSIZEMOVE is sent when the user releases the resize bars.
        // Here we reset everything based on the new window dimensions.
    case WM_EXITSIZEMOVE:
        if(renderer)
        {
            RECT clientRect;
            GetClientRect(hWnd, &clientRect);

            uint32_t width = clientRect.right - clientRect.left;
            uint32_t height = clientRect.bottom - clientRect.top;

			renderer->SetPaused(false);
			renderer->SetResizing(false);
			renderer->Start(); 
			renderer->OnResize(width, height);
        }

        break;

    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        if (renderer)
        {
            pushed = true;
			renderer->OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        break;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        if (renderer)
        {
            pushed = false;
			renderer->OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        break;
    case WM_MOUSEMOVE:
        if (renderer)
        {
            if (pushed)
            {
                renderer->OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            }
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

LRESULT LoadWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static wstring modelPath;
    static wstring textureDirPath;

    static float scale;
    static DirectX::XMFLOAT3 transl;
    static float angle;
    static DirectX::XMFLOAT3 rotationAxis;

    static bool animation;
    static bool transparency;

    static auto GetEditTextFloat = [](HWND hWnd, int resourceId, float& num, float defaultNum)
    {
        WCHAR buffer[1024];

        HWND edithWnd = GetDlgItem(hWnd, resourceId);
        GetWindowText(edithWnd, buffer, 1024);
        num = buffer[0] == 0 ? defaultNum : _wtof(buffer);
    };

    static auto GetEditTextWString = [](HWND hWnd, int resourceId, wstring& str)
    {
        WCHAR buffer[1024];

        HWND edithWnd = GetDlgItem(hWnd, resourceId);
        GetWindowText(edithWnd, buffer, 1024);
        str = buffer;
    };

    static auto GetButtonState = [](HWND hWnd, int resourceId, bool& state)
    {
        HWND radioButtonhWnd = GetDlgItem(hWnd, resourceId);
        state = (Button_GetState(radioButtonhWnd) == BST_CHECKED);
    };

    switch (message)
    {
    case WM_INITDIALOG:
        SetDlgItemText(hWnd, IDC_MODEL_NAME_EDIT, L"");
        SetDlgItemText(hWnd, IDC_MODEL_PATH_EDIT, L"");
        SetDlgItemText(hWnd, IDC_TEXTURE_PATH_EDIT, L"");
        SetDlgItemText(hWnd, IDC_SCALE, L"1.0");
        SetDlgItemText(hWnd, IDC_TRANSLATION_X, L"0.0");
        SetDlgItemText(hWnd, IDC_TRANSLATION_Y, L"0.0");
        SetDlgItemText(hWnd, IDC_TRANSLATION_Z, L"0.0");
        SetDlgItemText(hWnd, IDC_ROTATION, L"0.0");
        SetDlgItemText(hWnd, IDC_ROTATION_AXIS_X, L"0.0");
        SetDlgItemText(hWnd, IDC_ROTATION_AXIS_Y, L"1.0");
        SetDlgItemText(hWnd, IDC_ROTATION_AXIS_Z, L"0.0");

        break;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_MODEL_PATH_BUTTON:
        {
            using namespace Carol;

            try
            {
                ComPtr<IFileDialog> pfd;
                ThrowIfFailed(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(pfd.GetAddressOf())));

                DWORD dwOptions;
                ThrowIfFailed(pfd->GetOptions(&dwOptions));
                pfd->SetOptions(dwOptions | FOS_FILEMUSTEXIST);

                ThrowIfFailed(pfd->Show(hWnd));
                ComPtr<IShellItem> psi;
                LPWSTR path = NULL;

                ThrowIfFailed(pfd->GetResult(psi.GetAddressOf()));
                ThrowIfFailed(psi->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &path));

                SetDlgItemText(hWnd, IDC_MODEL_PATH_EDIT, path);
            }
            catch (DxException& e)
			{
				MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
				return 0;
			}
        }
         
            break;  

        case IDC_TEXTURE_PATH_BUTTON:
        {
            using namespace Carol;
            
            try
            {
                ComPtr<IFileDialog> pfd;
                ThrowIfFailed(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(pfd.GetAddressOf())));

                DWORD dwOptions;
                ThrowIfFailed(pfd->GetOptions(&dwOptions));
                pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);

                ThrowIfFailed(pfd->Show(hWnd));
                IShellItem* psi;
                LPWSTR path = NULL;

                ThrowIfFailed(pfd->GetResult(&psi));
                ThrowIfFailed(psi->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &path));

                SetDlgItemText(hWnd, IDC_TEXTURE_PATH_EDIT, path);
            }
            catch (DxException& e)
			{
				MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
				return 0;
			}
        }

            break;

        case IDC_LOAD_CANCEL:
            EndDialog(hWnd, 0);
            break;

        case IDC_LOAD_CONFIRM:
            GetEditTextWString(hWnd, IDC_MODEL_NAME_EDIT, loadModelName);
            GetEditTextWString(hWnd, IDC_MODEL_PATH_EDIT, modelPath);
            GetEditTextWString(hWnd, IDC_TEXTURE_PATH_EDIT, textureDirPath);

            GetEditTextFloat(hWnd, IDC_SCALE, scale, 1.0f);

            GetEditTextFloat(hWnd, IDC_TRANSLATION_X, transl.x, 0.0f);
            GetEditTextFloat(hWnd, IDC_TRANSLATION_Y, transl.y, 0.0f);
            GetEditTextFloat(hWnd, IDC_TRANSLATION_Z, transl.z, 0.0f);

            GetEditTextFloat(hWnd, IDC_ROTATION, angle, 0.0f);

            GetEditTextFloat(hWnd, IDC_ROTATION_AXIS_X, rotationAxis.x, 0.0f);
            GetEditTextFloat(hWnd, IDC_ROTATION_AXIS_Y, rotationAxis.y, 1.0f);
            GetEditTextFloat(hWnd, IDC_ROTATION_AXIS_Z, rotationAxis.z, 0.0f);

            GetButtonState(hWnd, IDC_CHECK_ANIMATION, animation);

            EnableWindow(GetDlgItem(hWnd, IDC_LOAD_CONFIRM), false);
            
            auto scaling = DirectX::XMMatrixScaling(scale, scale, scale);
            auto translation = DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&transl));
            auto rotation = DirectX::XMMatrixRotationAxis(DirectX::XMLoadFloat3(&rotationAxis), DirectX::XM_PI * angle / 180.f);
            auto world = scaling * rotation * translation;
            renderer->LoadModel(modelPath, textureDirPath, loadModelName, world, animation);

            if (animation)
            {
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ANIMATION), hWnd, AnimationWndProc);
            }

            //EnableWindow(GetDlgItem(hWnd, IDC_LOAD_CONFIRM), true);
            EndDialog(hWnd, 0);

            break;
        default:
            DefWindowProc(hWnd, message, wParam, lParam);
        }
        
        break;

    default:
        DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

LRESULT AnimationWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static auto GetSelectedAnimation = [](HWND hWnd, WPARAM wParam)
    {
        auto lbhWnd = GetDlgItem(hWnd, IDC_ANIMATION_LIST);
		int lbItem = SendMessage(lbhWnd, LB_GETCURSEL, 0, 0);

        WCHAR bufw[1024];
        SendMessage(lbhWnd, LB_GETTEXT, lbItem, (LPARAM)bufw);

        return wstring(bufw);
    };

    switch (message)
    {
    case WM_INITDIALOG:
    {
        auto lbhWnd = GetDlgItem(hWnd, IDC_ANIMATION_LIST);
        for (auto& aniName : renderer->GetAnimationNames(loadModelName))
        {
            SendMessage(lbhWnd, LB_ADDSTRING, 0, (LPARAM)aniName.data());
        }
    }

        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_ANIMATION_LIST:
            if (HIWORD(wParam) == LBN_DBLCLK)
            {
                renderer->SetAnimation(loadModelName, GetSelectedAnimation(hWnd, wParam));
                EndDialog(hWnd, 0);
            }

            break;

        default:
            DefWindowProc(hWnd, message, wParam, lParam);
        }

    default:
        DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

LRESULT DeleteWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static auto GetSelectedModel = [](HWND hWnd, WPARAM wParam)
    {
        auto lbhWnd = GetDlgItem(hWnd, IDC_DELETE_LIST);
		int lbItem = SendMessage(lbhWnd, LB_GETCURSEL, 0, 0);

        WCHAR bufw[1024];
        SendMessage(lbhWnd, LB_GETTEXT, lbItem, (LPARAM)bufw);

        return wstring(bufw);
    };

    switch (message)
    {
    case WM_INITDIALOG:
    {
        auto lbhWnd = GetDlgItem(hWnd, IDC_DELETE_LIST);
        for (auto& modelName : renderer->GetModelNames())
        {
            SendMessage(lbhWnd, LB_ADDSTRING, 0, (LPARAM)modelName.data());
        }
    }

        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_DELETE_LIST:
            if (HIWORD(wParam) == LBN_DBLCLK)
            {
                renderer->UnloadModel(GetSelectedModel(hWnd, wParam));
                EndDialog(hWnd, 0);
            }

            break;

        default:
            DefWindowProc(hWnd, message, wParam, lParam);
        }

    default:
        DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}
