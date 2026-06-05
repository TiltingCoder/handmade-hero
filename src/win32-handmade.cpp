#include <windows.h>
#include <stdint.h>

struct Win32OffscreenBuffer {
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int BytesPerPixel;
    int Pitch;
};

struct Win32WindowSize {
    int Width;
    int Height;
};

static bool Running;
static Win32OffscreenBuffer GBuffer;

static Win32WindowSize Win32GetWindowSize(HWND Window) {
    Win32WindowSize Result;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return Result;
}

static void RenderWeirdGradiant(Win32OffscreenBuffer Buffer, int XOffset, int YOffset) {
    uint8_t *Row = (uint8_t *)Buffer.Memory;
    for (int y = 0; y < Buffer.Height; y++) {
        uint32_t *Pixel = (uint32_t *)Row;
        for (int x = 0; x < Buffer.Width; x++) {
            /*
                Pixel in Memory: BB GG RR xx
                             0X  xx RR GG BB
            */
            uint8_t Blue = (uint8_t)x + XOffset;
            uint8_t Green = (uint8_t)y + YOffset;
            uint8_t Red = x + y;
            *Pixel++ = (Red << 16 | Green << 8 | Blue); // Blue
        }
        Row += Buffer.Pitch;
    }
}

static void Win32ResizeDIBSection(Win32OffscreenBuffer *Buffer, int Width, int Height) {
    if (Buffer->Memory) {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->BytesPerPixel = 4;

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    int BitmapMemorySize = (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
    Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;
}

static void Win32DisplayBufferInWindow(HDC DeviceContext, Win32OffscreenBuffer Buffer, int Width,
                                       int Height) {
    StretchDIBits(DeviceContext, 0, 0, Width, Height, 0, 0, Buffer.Width, Buffer.Height,
                  Buffer.Memory, &Buffer.Info, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
    LRESULT Result = 0;

    switch (Message) {
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
        Win32WindowSize Size = Win32GetWindowSize(Window);
        Win32DisplayBufferInWindow(DeviceContext, GBuffer, Size.Width, Size.Height);
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
    Win32ResizeDIBSection(&GBuffer, 1280, 720);

    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = MainWindowCallback;
    windowClass.hInstance = Instance;
    // windowClass.hIcon
    windowClass.lpszClassName = "HandmadeHeroWindowClass";
    if (RegisterClassA(&windowClass)) {
        HWND Window = CreateWindowExA(
            0, windowClass.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, Instance, 0);
        if (Window) {
            int XOffset = 0;
            int YOffset = 0;

            Running = true;
            while (Running) {
                MSG Message;
                while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE)) {
                    if (Message.message == WM_QUIT) {
                        Running = false;
                    }

                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                }

                RenderWeirdGradiant(GBuffer, XOffset++, YOffset++);
                HDC DeviceContext = GetDC(Window);
                Win32WindowSize Size = Win32GetWindowSize(Window);
                Win32DisplayBufferInWindow(DeviceContext, GBuffer, Size.Width, Size.Height);
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
