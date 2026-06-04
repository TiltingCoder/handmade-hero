#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    MessageBox(0, "Hello", "Handmade", MB_OK | MB_ICONINFORMATION);
    return 0;
}
