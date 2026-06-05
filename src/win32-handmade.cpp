#include <windows.h>
#include <stdint.h>

static bool Running;
static BITMAPINFO BitmapInfo;
static void *BitmapMemory;
static int BitmapWidth;
static int BitmapHeight;
static int BytesPerPixel = 4;

static void RenderWeirdGradiant(int XOffset, int YOffset) {
    int Pitch = BitmapWidth * BytesPerPixel;
    uint8_t *Row = (uint8_t *)BitmapMemory;
    for (int y = 0; y < BitmapHeight; y++) {
        uint32_t *Pixel = (uint32_t *)Row;
        for (int x = 0; x < BitmapWidth; x++) {
            /*
                Pixel in Memory: BB GG RR xx
                            0X   xx RR GG BB
            */
            uint8_t Blue = (uint8_t)x + XOffset;
            uint8_t Green = (uint8_t)y + YOffset;
            uint8_t Red = x + y;
            *Pixel++ = (Red << 16 | Green << 8 | Blue); // Blue
        }
        Row += Pitch;
    }
}

static void Win32ResizeDIBSection(int Width, int Height) {
    if (BitmapMemory) {
        VirtualFree(BitmapMemory, 0, MEM_RELEASE);
    }

    BitmapHeight = Height;
    BitmapWidth = Width;

    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biWidth = BitmapWidth;
    BitmapInfo.bmiHeader.biHeight = -BitmapHeight;
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;

    int BitmapMemorySize = (BitmapWidth * BitmapHeight) * BytesPerPixel;
    BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

    RenderWeirdGradiant(0, 0);
}

static void Win32UpdateWindow(HDC DeviceContext, RECT *ClientRect) {
    int WindowWidth = ClientRect->right - ClientRect->left;
    int WindowHeight = ClientRect->bottom - ClientRect->top;
    StretchDIBits(DeviceContext, 0, 0, WindowWidth, WindowHeight, 0, 0, BitmapWidth, BitmapHeight,
                  BitmapMemory, &BitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
    LRESULT Result = 0;

    switch (Message) {
    case WM_SIZE: {
        RECT ClientRect;
        GetClientRect(Window, &ClientRect);
        int Width = ClientRect.right - ClientRect.left;
        int Height = ClientRect.bottom - ClientRect.top;
        Win32ResizeDIBSection(Width, Height);
        InvalidateRect(Window, 0, FALSE);
    } break;
    case WM_DESTROY: {
        Running = false;
    } break;
    case WM_CLOSE: {
        Running = false;
    } break;
    case WM_ACTIVATEAPP: {
        OutputDebugStringA("WM_ACTIVATEAPP\n");
    } break;
    case WM_PAINT: {
        PAINTSTRUCT Paint;
        HDC DeviceContext = BeginPaint(Window, &Paint);

        RECT ClientRect;
        GetClientRect(Window, &ClientRect);

        Win32UpdateWindow(DeviceContext, &ClientRect);
        EndPaint(Window, &Paint);
    } break;
    default: {
        Result = DefWindowProcA(Window, Message, WParam, LParam);
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
    if (RegisterClassA(&windowClass)) {
        HWND Window = CreateWindowExA(
            0, windowClass.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, Instance, 0);
        if (Window) {
            Running = true;
            int XOffset = 0;
            int YOffset = 0;
            while (Running) {
                MSG Message;
                while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE)) {
                    if (Message.message == WM_QUIT) {
                        Running = false;
                    }

                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                }

                RenderWeirdGradiant(XOffset++, YOffset++);
                HDC DeviceContext = GetDC(Window);
                RECT ClientRect;
                GetClientRect(Window, &ClientRect);
                Win32UpdateWindow(DeviceContext, &ClientRect);
                ReleaseDC(Window, DeviceContext);
            }
        } else {
            OutputDebugStringA("Failed WindowHandle\n");
        }
    } else {
        OutputDebugStringA("Failed RegisterClass\n");
    }

    return 0;
}
