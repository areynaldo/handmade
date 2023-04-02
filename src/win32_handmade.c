#include <math.h>
#include <stdbool.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

typedef i32 b32;

#define internal static
#define persist static
#define global static

#define Pi32 3.14159265359f


#include <dsound.h>
#include <stdio.h>
#include <windows.h>
#include <xinput.h>

#include "handmade.c"
#include "win32_handmade.h"

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
Win32ProcessXInputDigitalButton(DWORD xInputButtonState,
                                HandmadeButtonState *oldState, DWORD buttonBit,
                                HandmadeButtonState *newState)
{
    newState->endedDown = ((xInputButtonState & buttonBit) == buttonBit);
    newState->halfTransitionCount = (oldState->endedDown != newState->endedDown) ? 1 : 0;
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

internal void
Win32ClearBuffer(Win32SoundOutput *soundOutput)
{
    void *region1;
    DWORD region1Size;
    void *region2;
    DWORD region2Size;
    if (SUCCEEDED(IDirectSoundBuffer_Lock(globalSecondaryBuffer,
                                          0, soundOutput->secondaryBufferSize,
                                          &region1, &region1Size,
                                          &region2, &region2Size,
                                          0)))
    {
        u8 *destSample = (u8 *)region1;
        for (DWORD byteIndex = 0; byteIndex < region1Size; byteIndex++)
        {
            *destSample++ = 0;
        }
        destSample = (u8 *)region2;
        for (DWORD byteIndex = 0; byteIndex < region2Size; byteIndex++)
        {
            *destSample++ = 0;
        }
        IDirectSoundBuffer_Unlock(globalSecondaryBuffer, region1, region1Size, region2, region2Size);
        IDirectSoundBuffer_Unlock(globalSecondaryBuffer, region1, region1Size, region2, region2Size);
    }
}

internal void
Win32FillSoundBuffer(Win32SoundOutput *soundOutput, DWORD byteToLock, DWORD bytesToWrite,
                     HandmadeSoundOutputBuffer *sourceBuffer)
{
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
        DWORD region1SampleCount = region1Size / soundOutput->bytesPerSample;
        i16 *sourceSample = sourceBuffer->samples;
        i16 *destSample = (i16 *)region1;
        for (DWORD sampleIndex = 0; sampleIndex < region1SampleCount; sampleIndex++)
        {
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;

            soundOutput->runningSampleIndex++;
        }
        destSample = (i16 *)region2;
        DWORD region2SampleCount = region2Size / soundOutput->bytesPerSample;
        for (DWORD sampleIndex = 0; sampleIndex < region2SampleCount; sampleIndex++)
        {
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;

            soundOutput->runningSampleIndex++;
        }

        IDirectSoundBuffer_Unlock(globalSecondaryBuffer, region1, region1Size, region2, region2Size);
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
    LARGE_INTEGER perfCountFrequencyResult;
    QueryPerformanceFrequency(&perfCountFrequencyResult);
    i64 perfCountFrequency = perfCountFrequencyResult.QuadPart;

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

            Win32SoundOutput soundOutput = {0};
            soundOutput.samplesPerSecond = 48000;
            soundOutput.bytesPerSample = sizeof(i16) * 2;
            soundOutput.secondaryBufferSize = 2 * soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
            soundOutput.runningSampleIndex = 0;
            soundOutput.latencySampleCount = soundOutput.samplesPerSecond / 15;

            Win32InitSound(window, soundOutput.samplesPerSecond, soundOutput.secondaryBufferSize);
            Win32ClearBuffer(&soundOutput);
            IDirectSoundBuffer_Play(globalSecondaryBuffer, 0, 0, DSBPLAY_LOOPING);
            i16 *samples = VirtualAlloc(0, soundOutput.secondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            // Since we specified CS_OWNDC, we can just
            // get one device context and use it forever because we
            // are not sharing it with anyone.
            HandmadeInput input[2] = {0};
            HandmadeInput* oldInput = &input[0];
            HandmadeInput* newInput = &input[1];
            LARGE_INTEGER lastCounter;
            QueryPerformanceCounter(&lastCounter);
            u64 lastCycleCount = __rdtsc();
            running = true;
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

                int maxControllerCount = XUSER_MAX_COUNT;
                if(maxControllerCount < ArrayCount(newInput->controllers))
                {
                    maxControllerCount = ArrayCount(newInput->controllers);
                }
                for (DWORD controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; controllerIndex++)
                {
                    HandmadeControllerInput *oldController = &oldInput->controllers[controllerIndex];
                    HandmadeControllerInput *newController = &newInput->controllers[controllerIndex];

                    XINPUT_STATE controllerState;
                    if (XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS)
                    {
                        XINPUT_GAMEPAD *pad = &controllerState.Gamepad;

                        bool up = pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        bool down = pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                        bool left = pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        bool right = pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                        // bool back = pad->wButtons & XINPUT_GAMEPAD_START;
                        // bool start = pad->wButtons & XINPUT_GAMEPAD_BACK;
                        Win32ProcessXInputDigitalButton(pad->wButtons,
                                                        &oldController->down, XINPUT_GAMEPAD_A,
                                                        &newController->down);
                        Win32ProcessXInputDigitalButton(pad->wButtons,
                                                        &oldController->down, XINPUT_GAMEPAD_B,
                                                        &newController->down);
                        Win32ProcessXInputDigitalButton(pad->wButtons,
                                                        &oldController->down, XINPUT_GAMEPAD_X,
                                                        &newController->down);
                        Win32ProcessXInputDigitalButton(pad->wButtons,
                                                        &oldController->down, XINPUT_GAMEPAD_Y,
                                                        &newController->down);
                        Win32ProcessXInputDigitalButton(pad->wButtons,
                                                        &oldController->down, XINPUT_GAMEPAD_LEFT_SHOULDER,
                                                        &newController->down);
                        Win32ProcessXInputDigitalButton(pad->wButtons,
                                                        &oldController->down, XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                                        &newController->down);
                        f32 x;
                        if(pad->sThumbLX < 0)
                        {
                            x = (f32)pad->sThumbLX / 32768.0f;
                        }
                        else
                        {
                            x = (f32)pad->sThumbLX / 32767.0f;
                        }

                        f32 y;
                        if(pad->sThumbLY < 0)
                        {
                            y = (f32)pad->sThumbLY / 32768.0f;
                        }
                        else
                        {
                            y = (f32)pad->sThumbLY / 32767.0f;
                        }


                        newController->startX = oldController->endX;
                        newController->startY = oldController->endY;

                        newController->minX = newController->maxX =  newController->endX = x;
                        newController->minY = newController->maxY =  newController->endY = y;

                        newController->isAnalog = true;
                    }
                    else
                    {
                    }
                }

                DWORD byteToLock;
                DWORD bytesToWrite;
                DWORD targetCursor;
                DWORD playCursor;
                DWORD writeCursor;
                b32 soundIsValid = false;
                if (SUCCEEDED(IDirectSoundBuffer_GetCurrentPosition(globalSecondaryBuffer,
                                                                    &playCursor, &writeCursor)))
                {
                    byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample) % soundOutput.secondaryBufferSize;

                    targetCursor = ((playCursor + (soundOutput.latencySampleCount * soundOutput.bytesPerSample)) % soundOutput.secondaryBufferSize);

                    if (byteToLock > targetCursor)
                    {
                        // Play cursor is behind
                        bytesToWrite = soundOutput.secondaryBufferSize - byteToLock; // Region 1
                        bytesToWrite += targetCursor;                                // Region 2
                    }
                    else
                    {
                        // Play cursor is in front
                        bytesToWrite = targetCursor - byteToLock;
                    }
                    soundIsValid = true;
                }

                HandmadeSoundOutputBuffer soundBuffer = {0};
                soundBuffer.samplesPerSecond = soundOutput.samplesPerSecond;
                soundBuffer.sampleCount = bytesToWrite / soundOutput.bytesPerSample;
                soundBuffer.samples = samples;

                HandmadeOffscreenBuffer buffer = {0};
                buffer.memory = globalBackBuffer.memory;
                buffer.width = globalBackBuffer.width;
                buffer.height = globalBackBuffer.height;
                buffer.pitch = globalBackBuffer.pitch;

                HandmadeUpdateAndRender(&newInput, &buffer, &soundBuffer);
                if (soundIsValid)
                {
                    Win32FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite, &soundBuffer);
                }

                Win32WindowDimension dimension = Win32GetWindowDimension(window);
                Win32DisplayBufferInWindow(&globalBackBuffer, deviceContext, dimension.width, dimension.height);

                LARGE_INTEGER endCounter;
                QueryPerformanceCounter(&endCounter);

                u64 endCycleCount = __rdtsc();
                i64 counterElapsed = endCounter.QuadPart - lastCounter.QuadPart;
                i64 cyclesElapsed = endCycleCount - lastCycleCount;
                f32 msPerFrame = 1000.0 * (f32)counterElapsed / (f32)perfCountFrequency;
                f32 fps = (f32)perfCountFrequency / (f32)counterElapsed;
                f32 megaCyclesPerFrame = (f32)cyclesElapsed / (1000.0 * 1000.0);
                /*
                char buffer[256];
                sprintf(buffer, "%.02fms/f %.02ff/s %.02fMc/f \n", msPerFrame, fps, megaCyclesPerFrame);
                OutputDebugStringA(buffer);
                */
                lastCounter = endCounter;
                lastCycleCount = __rdtsc();

                HandmadeInput *temp = newInput;
                newInput = oldInput;
                oldInput = temp;
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
