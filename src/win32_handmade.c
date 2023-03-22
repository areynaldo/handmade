#include <dsound.h>
#include <stdbool.h>
#include <stdint.h>
#include <windows.h>
#include <xinput.h>

#define internal static
#define persist static
#define global static

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef i32 b32;

typedef struct
{
    int width;
    int height;
} Win32WindowDimension;

typedef struct
{
    // PIxels are 32-bits wide,
    // Memory order  0x BB GG RR XX
    // Little Endian 0x XX RR GG BB
    BITMAPINFO info;
    void *memory;
    int width;
    int height;
    int pitch;
    int bytesPerPixel;
} Win32OffscreenBuffer;

global bool running;
global Win32OffscreenBuffer globalBackBuffer;
global IDirectSoundBuffer *globalSecondaryBuffer;

// TODO: review conventions
// XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(XInputGetStateFunc);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global XInputGetStateFunc *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(XInputSetStateFunc);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global XInputSetStateFunc *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

// Direct Sound Create
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(DirectSoundCreateFunc);

// Change pointers if lib loaded
internal void
Win32LoadXInput()
{
    HMODULE xInputLibrary = LoadLibraryA("Xinput1_4.dll");
    if (!xInputLibrary)
    {
        xInputLibrary = LoadLibraryA("Xinput1_3_0.dll");
    }
    if (!xInputLibrary)
    {
        xInputLibrary = LoadLibraryA("Xinput9_1_0.dll");
    }

    if (xInputLibrary)
    {
        XInputGetState = (XInputGetStateFunc *)GetProcAddress(xInputLibrary, "XInputGetState");
        if (!XInputGetState)
        {
            XInputGetState = XInputGetStateStub;
        }

        XInputSetState = (XInputSetStateFunc *)GetProcAddress(xInputLibrary, "XInputSetState");
        if (!XInputSetState)
        {
            XInputSetState = XInputSetStateStub;
        }
    }
    else
    {
        XInputGetState = XInputGetStateStub;
        XInputSetState = XInputSetStateStub;
    }
}

internal void
Win32InitSound(HWND window, i32 samplesPerSecond, i32 bufferSize)
{
    // Load the library
    HMODULE dSoundLibrary = LoadLibraryA("dsound.dll");

    if (dSoundLibrary)
    {
        DirectSoundCreateFunc *DirectSoundCreate = (DirectSoundCreateFunc *)GetProcAddress(dSoundLibrary, "DirectSoundCreate");
        IDirectSound *directSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &directSound, 0)))
        {
            WAVEFORMATEX waveFormat = {0};
            waveFormat.wFormatTag = WAVE_FORMAT_PCM;
            waveFormat.nChannels = 2;
            waveFormat.nSamplesPerSec = samplesPerSecond;
            waveFormat.wBitsPerSample = 16;
            waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8; // 4 under current settings
            waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;

            if (SUCCEEDED(IDirectSound_SetCooperativeLevel(directSound, window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC bufferDescription = {0};
                bufferDescription.dwSize = sizeof(bufferDescription);
                bufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

                IDirectSoundBuffer *primaryBuffer;
                if (SUCCEEDED(IDirectSound_CreateSoundBuffer(directSound, &bufferDescription, &primaryBuffer, 0)))
                {
                    if (SUCCEEDED(IDirectSoundBuffer_SetFormat(primaryBuffer, &waveFormat)))
                    {
                    }
                }
            }
            DSBUFFERDESC bufferDescription = {0};
            bufferDescription.dwSize = sizeof(bufferDescription);
            bufferDescription.dwBufferBytes = bufferSize;
            bufferDescription.lpwfxFormat = &waveFormat;

            if (SUCCEEDED(IDirectSound_CreateSoundBuffer(directSound, &bufferDescription, &globalSecondaryBuffer, 0)))
            {
                if (SUCCEEDED(IDirectSoundBuffer_SetFormat(globalSecondaryBuffer, &waveFormat)))
                {
                }
            }
        }
    }
}

internal Win32WindowDimension
Win32GetWindowDimension(HWND window)
{
    Win32WindowDimension result;

    RECT clientRect;

    GetClientRect(window, &clientRect);
    result.width = clientRect.right - clientRect.left;
    result.height = clientRect.bottom - clientRect.top;
    return result;
}

internal void
RenderWeirdGradient(Win32OffscreenBuffer *buffer, int xOffset, int yOffset)
{
    u8 *row = (u8 *)buffer->memory;
    for (int y = 0; y < buffer->height; y++)
    {
        u32 *pixel = (u32 *)row;
        for (int x = 0; x < buffer->width; x++)
        {
            // Pixel Memory BB GG RR XX
            u8 red = 0;
            u8 green = (u8)(y + yOffset);
            u8 blue = (u8)(x + xOffset);

            *pixel++ = red << 16 | green << 8 | blue;
        }
        row += buffer->pitch;
    }
}

internal void
Win32ResizeDIBSection(Win32OffscreenBuffer *buffer, int width, int height)
{

    if (buffer->memory)
    {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }
    buffer->width = width;
    buffer->height = height;
    buffer->bytesPerPixel = 4;
    buffer->pitch = buffer->bytesPerPixel * buffer->width;

    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = buffer->width;
    buffer->info.bmiHeader.biHeight = -buffer->height; // negative value to down pitch
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;
    buffer->info.bmiHeader.biCompression = BI_RGB;

    int bitmapMemorySize = buffer->bytesPerPixel * (width * height);

    buffer->memory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

internal void
Win32DisplayBufferInWindow(Win32OffscreenBuffer *buffer, HDC deviceContext, int windowWidth, int windowHeight)
{
    StretchDIBits(deviceContext,
                  0, 0, windowWidth, windowHeight,
                  0, 0, buffer->width, buffer->height,
                  buffer->memory,
                  &buffer->info,
                  DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK
Win32MainWindowCallback(HWND window,
                        UINT message,
                        WPARAM wParam,
                        LPARAM lParam)
{
    LRESULT result = 0;
    switch (message)
    {
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
        bool isDown = ((lParam & (1 << 31)) == 0);
        bool wasDown = ((lParam & (1 << 30)) != 0);
        u32 vKCode = wParam;
        if (isDown != wasDown)
        {
            if (vKCode == 'W')
            {
            }
            else if (vKCode == 'A')
            {
            }
            else if (vKCode == 'S')
            {
            }
            else if (vKCode == 'D')
            {
            }
            else if (vKCode == 'Q')
            {
            }
            else if (vKCode == 'E')
            {
            }
            else if (vKCode == VK_UP)
            {
            }
            else if (vKCode == VK_DOWN)
            {
            }
            else if (vKCode == VK_LEFT)
            {
            }
            else if (vKCode == VK_RIGHT)
            {
            }
            else if (vKCode == VK_ESCAPE)
            {
            }
            else if (vKCode == VK_SPACE)
            {
            }

            b32 altKeyWasDown = (lParam & (1 << 29));
            if ((vKCode == VK_F4) && altKeyWasDown)
            {
                running = false;
            }
        }
    }
    break;

    case WM_SIZE:
    {
    }
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT paint;
        HDC deviceContext = BeginPaint(window, &paint);
        Win32WindowDimension dimension = Win32GetWindowDimension(window);
        Win32DisplayBufferInWindow(&globalBackBuffer, deviceContext, dimension.width, dimension.height);
        EndPaint(window, &paint);
    }
    break;

    case WM_ACTIVATEAPP:
    {
    }
    break;

    case WM_DESTROY:
    {
        running = false;
    }
    break;

    case WM_CLOSE:
    {
        running = false;
    }
    break;

    default:
    {
        result = DefWindowProc(window, message, wParam, lParam);
    }
    break;
    }

    return result;
}

int CALLBACK
WinMain(HINSTANCE instance,
        HINSTANCE previousInstance,
        LPSTR commandLine,
        int showCommandLine)
{
    Win32LoadXInput();

    WNDCLASSA windowClass = {0};
    windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = Win32MainWindowCallback;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = "HandmadeWindowClass";

    if (RegisterClassA(&windowClass))
    {
        HWND window = CreateWindowExA(0, windowClass.lpszClassName, "Handmade",
                                      WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                      CW_USEDEFAULT, CW_USEDEFAULT,
                                      CW_USEDEFAULT, CW_USEDEFAULT,
                                      0, 0, instance, 0);
        if (window)
        {
            HDC deviceContext = GetDC(window);
            Win32ResizeDIBSection(&globalBackBuffer, 1280, 720);

            int samplesPerSecond = 48000;
            int bytesPerSample = sizeof(i16) * 2;
            int secondaryBufferSize = 2 * samplesPerSecond * bytesPerSample;
            u32 runningSampleIndex = 0;
            int toneHz = 1000;
            int squareWavePeriod = samplesPerSecond / toneHz;
            int halfSquareWavePeriod = squareWavePeriod / 2;
            int toneVolume = 3000;

            Win32InitSound(window, samplesPerSecond, secondaryBufferSize);
            IDirectSoundBuffer_Play(globalSecondaryBuffer, 0, 0, DSBPLAY_LOOPING);
            running = true;
            // Since we specified CS_OWNDC, we can just
            // get one device context and use it forever because we
            // are not sharing it with anyone.
            int xOffset = 0;
            int yOffset = 0;
            while (running)
            {
                MSG message;
                while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE))
                {
                    if (message.message == WM_QUIT)
                    {
                        running = false;
                    }

                    TranslateMessage(&message);
                    DispatchMessageA(&message);
                }
                // TODO: should we poll this more frequently?
                for (DWORD controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; controllerIndex++)
                {
                    XINPUT_STATE controllerState;
                    if (XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS)
                    {
                        XINPUT_GAMEPAD *pad = &controllerState.Gamepad;

                        bool up = pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        bool down = pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                        bool left = pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        bool right = pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                        bool back = pad->wButtons & XINPUT_GAMEPAD_START;
                        bool start = pad->wButtons & XINPUT_GAMEPAD_BACK;
                        bool leftShoulder = pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
                        bool rightShoulder = pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
                        bool a = pad->wButtons & XINPUT_GAMEPAD_A;
                        bool b = pad->wButtons & XINPUT_GAMEPAD_B;
                        bool x = pad->wButtons & XINPUT_GAMEPAD_X;
                        bool y = pad->wButtons & XINPUT_GAMEPAD_Y;

                        i16 stickX = pad->sThumbLX;
                        i16 stickY = pad->sThumbLY;
                        xOffset -= stickX >> 12;
                        yOffset += stickY >> 12;
                    }
                    else
                    {
                    }
                }
                // XINPUT_VIBRATION vibration;
                // vibration.wLeftMotorSpeed = 60000;
                // vibration.wRightMotorSpeed = 60000;
                // XInputSetState(0, &vibration);
                RenderWeirdGradient(&globalBackBuffer, xOffset, yOffset);


                DWORD playCursor;
                DWORD writeCursor;
                if (SUCCEEDED(IDirectSoundBuffer_GetCurrentPosition(globalSecondaryBuffer,
                                                                    &playCursor, &writeCursor)))
                {
                    DWORD byteToLock = runningSampleIndex * bytesPerSample % secondaryBufferSize;
                    DWORD bytesToWrite;
                    if(byteToLock > playCursor)
                    {
                        // Play cursor is behind
                        bytesToWrite = secondaryBufferSize - byteToLock; // Region 1
                        bytesToWrite += playCursor;                      // Region 2
                    }
                    else
                    {
                        // Play cursor is in front
                        bytesToWrite = playCursor - byteToLock;
                    }

                    void *region1;
                    DWORD region1Size;
                    void *region2;
                    DWORD region2Size;
                    if (SUCCEEDED(IDirectSoundBuffer_Lock(globalSecondaryBuffer,
                                                          byteToLock, bytesToWrite,
                                                          &region1, &region1Size,
                                                          &region2, &region2Size,
                                                          0)))
                    {
                        i16 *sampleOut = (i16 *)region1;
                        DWORD region1SampleCount = region1Size / bytesPerSample;
                        for (DWORD sampleIndex = 0; sampleIndex < region1SampleCount; sampleIndex++)
                        {
                            i16 sampleValue = ((runningSampleIndex++ / halfSquareWavePeriod) % 2) ? toneVolume : -toneVolume;
                            *sampleOut = sampleValue;
                            sampleOut++;
                            *sampleOut = sampleValue;
                            sampleOut++;
                        }
                        sampleOut = (i16 *)region2;
                        DWORD region2SampleCount = region2Size / bytesPerSample;
                        for (DWORD sampleIndex = 0; sampleIndex < region2SampleCount; sampleIndex++)
                        {
                            i16 sampleValue = ((runningSampleIndex++ / halfSquareWavePeriod) % 2) ? toneVolume : -toneVolume;
                            *sampleOut = sampleValue;
                            sampleOut++;
                            *sampleOut = sampleValue;
                            sampleOut++;
                        }

                        IDirectSoundBuffer_Unlock(globalSecondaryBuffer, region1, region1Size, region2, region2Size);
                    }
                }

                Win32WindowDimension dimension = Win32GetWindowDimension(window);
                Win32DisplayBufferInWindow(&globalBackBuffer, deviceContext, dimension.width, dimension.height);
            }
        }
        else
        {
        }
    }
    else
    {
    }

    return 0;
}
