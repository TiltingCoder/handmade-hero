#include <windows.h>
#include <stdint.h>
#include <Xinput.h>
#include <libloaderapi.h>
#include <dsound.h>

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

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }
static x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }
static x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

static bool Running;
static Win32OffscreenBuffer GBuffer;
static LPDIRECTSOUNDBUFFER SecondaryBuffer;

static void Win32LoadXInput() {
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
    if (!XInputLibrary) {
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }
    if (XInputLibrary) {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        if (!XInputGetState) {
            XInputGetState = XInputGetStateStub;
        }
        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
        if (!XInputSetState) {
            XInputSetState = XInputSetStateStub;
        }
    }
}

static void Win32InitDSound(HWND Window, int32_t SamplePerSec, int32_t BufferSize) {
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
    if (DSoundLibrary) {
        direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
        LPDIRECTSOUND DirectSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))) {
            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))) {
                WAVEFORMATEX WaveFormat = {};
                WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
                WaveFormat.nChannels = 2;
                WaveFormat.nSamplesPerSec = SamplePerSec;
                WaveFormat.wBitsPerSample = 16;
                WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
                WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
                WaveFormat.cbSize = 0;

                DSBUFFERDESC PrimBuffDesc = {};
                PrimBuffDesc.dwSize = sizeof(PrimBuffDesc);
                PrimBuffDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&PrimBuffDesc, &PrimaryBuffer, 0))) {
                    if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat))) {

                    } else {
                        OutputDebugStringA("PrimaryBuffer SetFormat Failed\n");
                    }
                } else {
                    OutputDebugStringA("PrimaryBuffer CreateSoundBuffer Failed\n");
                }

                DSBUFFERDESC SecBuffDesc = {};
                SecBuffDesc.dwSize = sizeof(SecBuffDesc);
                SecBuffDesc.dwFlags = 0;
                SecBuffDesc.dwBufferBytes = BufferSize;
                SecBuffDesc.lpwfxFormat = &WaveFormat;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&SecBuffDesc, &SecondaryBuffer, 0))) {
                } else {
                    OutputDebugStringA("SecondaryBuffer CreateSoundBuffer Failed\n");
                }
            } else {
                OutputDebugStringA("SetCooperativeLevel Failed\n");
            }
        } else {
            OutputDebugStringA("No DirectSoundCreate\n");
        }
    }
}

static Win32WindowSize Win32GetWindowSize(HWND Window) {
    Win32WindowSize Result;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return Result;
}

static void RenderWeirdGradiant(Win32OffscreenBuffer *Buffer, int XOffset, int YOffset) {
    uint8_t *Row = (uint8_t *)Buffer->Memory;
    for (int y = 0; y < Buffer->Height; y++) {
        uint32_t *Pixel = (uint32_t *)Row;
        for (int x = 0; x < Buffer->Width; x++) {
            /*
                Pixel in Memory: BB GG RR xx
                              0X xx RR GG BB
            */
            uint8_t Blue = (uint8_t)x + XOffset;
            uint8_t Green = (uint8_t)y + YOffset;
            uint8_t Red = x + y;
            *Pixel++ = (Red << 16 | Green << 8 | Blue); // Blue
        }
        Row += Buffer->Pitch;
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

static void Win32DisplayBufferInWindow(HDC DeviceContext, Win32OffscreenBuffer *Buffer, int Width, int Height) { StretchDIBits(DeviceContext, 0, 0, Width, Height, 0, 0, Buffer->Width, Buffer->Height, Buffer->Memory, &Buffer->Info, DIB_RGB_COLORS, SRCCOPY); }

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

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP: {
        bool WasDown = !!(LParam & 1 << 30);
        bool IsDown = !(LParam & 1 << 31);

        if (WasDown != IsDown) {
            switch (WParam) {
            case 'W': {
                OutputDebugStringA("W\n");
            } break;
            case 'A': {
                OutputDebugStringA("A\n");
            } break;
            case 'S': {
                OutputDebugStringA("S\n");
            } break;
            case 'D': {
                OutputDebugStringA("D\n");
            } break;
            case 'Q': {
                OutputDebugStringA("Q\n");
            } break;
            case 'E': {
                OutputDebugStringA("E\n");
            } break;
            case VK_UP: {
                OutputDebugStringA("UP\n");
            } break;
            case VK_DOWN: {
                OutputDebugStringA("DW\n");
            } break;
            case VK_LEFT: {
                OutputDebugStringA("LT\n");
            } break;
            case VK_RIGHT: {
                OutputDebugStringA("RT\n");
            } break;
            case VK_ESCAPE: {
                OutputDebugStringA("ESC\n");
            } break;
            case VK_SPACE: {
                OutputDebugStringA("SPACE ");
                if (IsDown) {
                    OutputDebugStringA("IS DOWN ");
                }

                if (WasDown) {
                    OutputDebugStringA("WAS DOWN");
                }

                OutputDebugStringA("\n");
            } break;
            }
        }

        if (WParam == VK_F4 && !!(LParam & 1 << 29)) {
            Running = false;
        }
    } break;

    case WM_PAINT: {
        PAINTSTRUCT Paint;
        HDC DeviceContext = BeginPaint(Window, &Paint);
        Win32WindowSize Size = Win32GetWindowSize(Window);
        Win32DisplayBufferInWindow(DeviceContext, &GBuffer, Size.Width, Size.Height);
        EndPaint(Window, &Paint);
    } break;

    default: {
        Result = DefWindowProcA(Window, Message, WParam, LParam);
    } break;
    }

    return Result;
}

int WINAPI WinMain(HINSTANCE Instance, HINSTANCE, LPSTR, int) {
    Win32LoadXInput();
    WNDCLASSA windowClass = {};
    Win32ResizeDIBSection(&GBuffer, 1280, 720);

    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = MainWindowCallback;
    windowClass.hInstance = Instance;
    // windowClass.hIcon
    windowClass.lpszClassName = "HandmadeHeroWindowClass";
    if (RegisterClassA(&windowClass)) {
        HWND Window = CreateWindowExA(0, windowClass.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, Instance, 0);
        if (Window) {
            HDC DeviceContext = GetDC(Window);

            int XOffset = 0;
            int YOffset = 0;

            int SamplePerSec = 48000;
            int ToneHz = 256;
            int ToneVolume = 100;
            uint32_t RunningSampleIndex = 0;
            int SquareWavePeriod = SamplePerSec / ToneHz;
            int SquareWaveHalfPeriod = SquareWavePeriod / 2;
            int BytesPerSample = sizeof(int16_t) * 2;
            int BufferSize = SamplePerSec * BytesPerSample;

            Win32InitDSound(Window, SamplePerSec, BufferSize);
            SecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

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

                for (DWORD CtrlIndex = 0; CtrlIndex < XUSER_MAX_COUNT; CtrlIndex++) {
                    XINPUT_STATE CtrlState;
                    if (XInputGetState(CtrlIndex, &CtrlState) == ERROR_SUCCESS) {
                        XINPUT_GAMEPAD *GamePad = &CtrlState.Gamepad;

                        bool ButtonStart = (GamePad->wButtons & XINPUT_GAMEPAD_START);
                        bool ButtonBack = (GamePad->wButtons & XINPUT_GAMEPAD_BACK);
                        bool ButtonA = (GamePad->wButtons & XINPUT_GAMEPAD_A);
                        bool ButtonB = (GamePad->wButtons & XINPUT_GAMEPAD_B);
                        bool ButtonX = (GamePad->wButtons & XINPUT_GAMEPAD_X);
                        bool ButtonY = (GamePad->wButtons & XINPUT_GAMEPAD_Y);

                        bool DPadUp = (GamePad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool DPadDown = (GamePad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool DPadLeft = (GamePad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool DPadRight = (GamePad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

                        bool ShoulderRight = (GamePad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        bool ShoulderLeft = (GamePad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);

                        int16_t StickX = GamePad->sThumbLX;
                        int16_t StickY = GamePad->sThumbLY;

                        XOffset += StickX;
                        YOffset += StickY;
                    } else {
                        // OutputDebugStringA("XInputGetState Failed");
                    }
                }

                RenderWeirdGradiant(&GBuffer, XOffset, YOffset);

                DWORD PlayCursor;
                DWORD WriteCursor;
                if (SUCCEEDED(SecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor))) {
                    DWORD BytesToLock = RunningSampleIndex * BytesPerSample % BufferSize;
                    DWORD BytesToWrite;
                    if (BytesToLock > PlayCursor) {
                        BytesToWrite = BufferSize - BytesToLock;
                        BytesToWrite += PlayCursor;
                    } else {
                        BytesToWrite = PlayCursor - BytesToLock;
                    }

                    VOID *Region1;
                    DWORD Region1Size;
                    VOID *Region2;
                    DWORD Region2Size;

                    if (SUCCEEDED(SecondaryBuffer->Lock(BytesToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0))) {
                        int16_t *SampleOut = (int16_t *)Region1;
                        DWORD Region1SampleCount = Region1Size / BytesPerSample;
                        for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; SampleIndex++) {
                            int16_t SampleValue = (RunningSampleIndex++ / SquareWaveHalfPeriod) % 2 ? ToneVolume : -ToneVolume;
                            *SampleOut++ = SampleValue;
                            *SampleOut++ = SampleValue;
                        }

                        SampleOut = (int16_t *)Region2;
                        DWORD Region2SampleCount = Region2Size / BytesPerSample;
                        for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; SampleIndex++) {
                            int16_t SampleValue = (RunningSampleIndex++ / SquareWaveHalfPeriod) % 2 ? ToneVolume : -ToneVolume;
                            *SampleOut++ = SampleValue;
                            *SampleOut++ = SampleValue;
                        }

                        SecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
                    } else {
                        OutputDebugStringA("Lock Failed\n");
                    }
                } else {
                    OutputDebugStringA("GetCurrentPosition Failed\n");
                }

                Win32WindowSize Size = Win32GetWindowSize(Window);
                Win32DisplayBufferInWindow(DeviceContext, &GBuffer, Size.Width, Size.Height);
            }
            ReleaseDC(Window, DeviceContext);
        } else {
            OutputDebugStringA("Failed WindowHandle\n");
        }
    } else {
        OutputDebugStringA("Failed RegisterClass\n");
    }

    return 0;
}
