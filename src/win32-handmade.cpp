#include <windows.h>
#include <stdint.h>
#include <Xinput.h>
#include <libloaderapi.h>
#include <dsound.h>
#include <math.h>
#include <stdio.h>

#define PI 3.14159265358979323846f

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

struct Win32SoundOutput {
    float TSine;
    uint32_t RunningSampleIndex;
    int BufferSize;
    int SampleLatency;
    int SamplePerSec;
    int ToneHz;
    int ToneVolume;
    int WavePeriod;
    int BytesPerSample;
};

#define XINPUTGETSTATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef XINPUTGETSTATE(xInputGetState_t);
XINPUTGETSTATE(XInputGetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }
static xInputGetState_t *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define XINPUTSETSTATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef XINPUTSETSTATE(xInputSetState_t);
XINPUTSETSTATE(XInputSetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }
static xInputSetState_t *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECTSOUNDCREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECTSOUNDCREATE(directSoundCreate_t);

static bool Running;
static Win32OffscreenBuffer GBuffer;
static LPDIRECTSOUNDBUFFER SecondaryBuffer;

static void Win32LoadXInput() {
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
    if (!XInputLibrary) {
        XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
    }
    if (!XInputLibrary) {
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }
    if (XInputLibrary) {
        XInputGetState = (xInputGetState_t *)GetProcAddress(XInputLibrary, "XInputGetState");
        if (!XInputGetState) {
            XInputGetState = XInputGetStateStub;
        }
        XInputSetState = (xInputSetState_t *)GetProcAddress(XInputLibrary, "XInputSetState");
        if (!XInputSetState) {
            XInputSetState = XInputSetStateStub;
        }
    }
}

static void Win32InitDSound(HWND Window, int32_t SamplePerSec, int32_t BufferSize) {
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
    if (DSoundLibrary) {
        directSoundCreate_t *DirectSoundCreate = (directSoundCreate_t *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
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
                SecBuffDesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
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

static void Win32FillSoundBuffer(Win32SoundOutput *SoundOutput, DWORD BytesToLock, DWORD BytesToWrite) {
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;

    if (SUCCEEDED(SecondaryBuffer->Lock(BytesToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0))) {
        int16_t *SampleOut = (int16_t *)Region1;
        DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
        for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; SampleIndex++) {
            float SineValue = sinf(SoundOutput->TSine);
            int16_t SampleValue = SineValue * SoundOutput->ToneVolume;
            SoundOutput->TSine += 2 * PI * 1.0f / (float)SoundOutput->WavePeriod;
            *SampleOut++ = SampleValue;
            *SampleOut++ = SampleValue;
            ++SoundOutput->RunningSampleIndex;
        }

        SampleOut = (int16_t *)Region2;
        DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; SampleIndex++) {
            float SineValue = sinf(SoundOutput->TSine);
            int16_t SampleValue = SineValue * SoundOutput->ToneVolume;
            SoundOutput->TSine += 2 * PI * 1.0f / (float)SoundOutput->WavePeriod;
            *SampleOut++ = SampleValue;
            *SampleOut++ = SampleValue;
            ++SoundOutput->RunningSampleIndex;
        }

        SecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    } else {
        // OutputDebugStringA("Lock Failed\n");
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

static void Win32DisplayBufferInWindow(HDC DeviceContext, Win32OffscreenBuffer *Buffer, int Width, int Height) {
    StretchDIBits(DeviceContext, 0, 0, Width, Height, 0, 0, Buffer->Width, Buffer->Height, Buffer->Memory, &Buffer->Info,
                  DIB_RGB_COLORS, SRCCOPY);
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
    LARGE_INTEGER PerfCounterFrequency;
    QueryPerformanceFrequency(&PerfCounterFrequency);

    Win32LoadXInput();
    WNDCLASSA windowClass = {};
    Win32ResizeDIBSection(&GBuffer, 1280, 720);

    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = MainWindowCallback;
    windowClass.hInstance = Instance;
    // windowClass.hIcon
    windowClass.lpszClassName = "HandmadeHeroWindowClass";

    if (RegisterClassA(&windowClass)) {
        HWND Window = CreateWindowExA(0, windowClass.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, Instance, 0);
        if (Window) {
            HDC DeviceContext = GetDC(Window);

            int XOffset = 0;
            int YOffset = 0;

            Win32SoundOutput SoundOutput = {};
            SoundOutput.SamplePerSec = 48000;
            SoundOutput.ToneHz = 256;
            SoundOutput.ToneVolume = 100;
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.WavePeriod = SoundOutput.SamplePerSec / SoundOutput.ToneHz;
            SoundOutput.BytesPerSample = sizeof(int16_t) * 2;
            SoundOutput.BufferSize = SoundOutput.SamplePerSec * SoundOutput.BytesPerSample;
            SoundOutput.SampleLatency = SoundOutput.SamplePerSec / 15;

            Win32InitDSound(Window, SoundOutput.SamplePerSec, SoundOutput.BufferSize);
            Win32FillSoundBuffer(&SoundOutput, 0, SoundOutput.SampleLatency * SoundOutput.BytesPerSample);
            SecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            uint64_t LastCycleCount = __rdtsc();
            LARGE_INTEGER LastCounter;
            QueryPerformanceCounter(&LastCounter);

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

                        SoundOutput.ToneHz += StickX / 4096;
                        SoundOutput.WavePeriod = SoundOutput.SamplePerSec / SoundOutput.ToneHz;

                        XOffset += StickX / 4096;
                        YOffset += StickY / 4096;
                    } else {
                        // OutputDebugStringA("XInputGetState Failed");
                    }
                }

                RenderWeirdGradiant(&GBuffer, XOffset, YOffset);

                DWORD PlayCursor;
                DWORD WriteCursor;
                if (SUCCEEDED(SecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor))) {
                    DWORD BytesToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.BufferSize;
                    DWORD TargetCursor =
                        (PlayCursor + (SoundOutput.SampleLatency * SoundOutput.BytesPerSample)) % SoundOutput.BufferSize;
                    DWORD BytesToWrite = 0;
                    if (BytesToLock > TargetCursor) {
                        BytesToWrite = SoundOutput.BufferSize - BytesToLock;
                        BytesToWrite += TargetCursor;
                    } else {
                        BytesToWrite = TargetCursor - BytesToLock;
                    }

                    Win32FillSoundBuffer(&SoundOutput, BytesToLock, BytesToWrite);
                } else {
                    OutputDebugStringA("GetCurrentPosition Failed\n");
                }

                Win32WindowSize Size = Win32GetWindowSize(Window);
                Win32DisplayBufferInWindow(DeviceContext, &GBuffer, Size.Width, Size.Height);

                uint64_t EndCycleCount = __rdtsc();
                LARGE_INTEGER EndCounter;
                QueryPerformanceCounter(&EndCounter);

                uint64_t CyclesElepased = EndCycleCount - LastCycleCount;
                uint64_t CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
                float Perf = (1000.f * (float)CounterElapsed) / (float)PerfCounterFrequency.QuadPart;
                float Fps = (float)PerfCounterFrequency.QuadPart / (float)CounterElapsed;
                float Mcpf = (float)CyclesElepased / (1000.f * 1000.f);
                char Buffer[1024];
                sprintf(Buffer, "%.2f ms/f, %.2f f/s, %.2f mc/f\n", Perf, Fps, Mcpf);
                OutputDebugStringA(Buffer);

                LastCycleCount = EndCycleCount;
                LastCounter = EndCounter;
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
