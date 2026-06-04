#include <windows.h>
#include <wingdi.h>
#include <winuser.h>

LRESULT CALLBACK MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
    LRESULT Result = 0;

    switch (Message) {
    case WM_SIZE: {
        OutputDebugStringA("WM_SIZE\n");
    } break;
    case WM_DESTROY: {
        OutputDebugStringA("WM_DESTROY\n");
    } break;
    case WM_CLOSE: {
        OutputDebugStringA("WM_CLOSE\n");
    } break;
    case WM_ACTIVATEAPP: {
        OutputDebugStringA("WM_ACTIVATEAPP\n");
    } break;
    case WM_PAINT: {
        PAINTSTRUCT Paint;
        HDC DeviceContext = BeginPaint(Window, &Paint);
        int X = Paint.rcPaint.left;
        int Y = Paint.rcPaint.top;
        int Widht = Paint.rcPaint.right - Paint.rcPaint.left;
        int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
        static DWORD Operation = WHITENESS;
        PatBlt(DeviceContext, X, Y, Widht, Height, Operation);
        if (Operation == WHITENESS) {
            Operation = BLACKNESS;
        } else {
            Operation = WHITENESS;
        }
        EndPaint(Window, &Paint);
    } break;
    default: {
        OutputDebugStringA("DEFAULT\n");
        Result = DefWindowProc(Window, Message, WParam, LParam);
    } break;
    }

    return Result;
}

int WINAPI WinMain(HINSTANCE Instance, HINSTANCE, LPSTR, int) {
    WNDCLASS windowClass = {};
    windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = MainWindowCallback;
    windowClass.hInstance = Instance;
    // windowClass.hIcon
    windowClass.lpszClassName = "HandmadeHeroWindowClass";
    if (RegisterClass(&windowClass)) {
        HWND WindowHandle = CreateWindowEx(
            0,
            windowClass.lpszClassName,
            "Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            Instance,
            0
        );
        if (WindowHandle) {
            MSG Message;
            for (;;) {
                BOOL MessageResult = GetMessage(&Message, 0, 0, 0);
                if (MessageResult > 0) {
                    TranslateMessage(&Message);
                    DispatchMessage(&Message);
                } else {
                    break;
                }
            }
        } else {
            OutputDebugStringA("Failed WindowHandle\n");
        }
    } else {
        OutputDebugStringA("Failed RegisterClass\n");
    }

    return 0;
}
