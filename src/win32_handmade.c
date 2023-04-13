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

internal void
DEBUGPlatformFreeFileMemory(void *memory)
{
    if (memory)
    {
        VirtualFree(memory, 0, MEM_RELEASE);
    }
}

internal DebugReadFileResult
DEBUGPlatformReadEntireFile(char *fileName)
{
    DebugReadFileResult result = {0};
    HANDLE fileHandle = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if(fileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER fileSize;
        if(GetFileSizeEx(fileHandle, &fileSize))
        {
            u32 fileSize32 = SafeTruncateUInt64(fileSize.QuadPart);
            result.contents = VirtualAlloc(0, fileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            DWORD bytesRead;
            if(result.contents)
            {
                if(ReadFile(fileHandle, result.contents, fileSize32, &bytesRead, 0) &&
                  (fileSize.QuadPart == bytesRead))
                {
                    result.contentsSize = bytesRead;
                }
                else
                {
                    // Read failed
                    DEBUGPlatformFreeFileMemory(result.contents);
                    result.contents = 0;
                }
            }
            else
            {
                // Memory allocation failed
            }
        }
        CloseHandle(fileHandle);
    }
    else
    {
        // Handle creation failed
    }

    return result;
}

internal b32
DEBUGPlatformWriteEntireFile(char *fileName, u32 memorySize, void *memory)
{
    b32 result = false;

    HANDLE fileHandle = CreateFileA(fileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if(fileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD bytesWritten;
        if(WriteFile(fileHandle, memory, memorySize, &bytesWritten, 0))
        {
            result = (bytesWritten == memorySize);
        }
        else
        {
            // Write failed
        }
        CloseHandle(fileHandle);
    }
    else
    {
        // Handle creation failed
    }
            
    return result;
}

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
Win32ProcessKeyboardMessage(HandmadeButtonState *newState, b32 isDown)
{
    Assert(newState->endedDown != isDown);
    newState->endedDown = isDown;
    newState->halfTransitionCount++;
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
        Assert(!"Keyboard input came in through a non-dispatch message!");
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

internal void
Win32ProcessPendingMessages(HandmadeControllerInput *keyboardController)
{
    MSG message;
    while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE))
    {
        switch(message.message)
        {
            case WM_QUIT:
            {
                running = false;
            } break;
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                bool isDown = ((message.lParam & (1 << 31)) == 0);
                bool wasDown = ((message.lParam & (1 << 30)) != 0);
                u32 vKCode = (u32)message.wParam;
                if (isDown != wasDown)
                {
                    if (vKCode == 'W')
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->moveUp, isDown);
                    }
                    else if (vKCode == 'A')
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->moveLeft, isDown);
                    }
                    else if (vKCode == 'S')
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->moveDown, isDown);
                    }
                    else if (vKCode == 'D')
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->moveRight, isDown);
                    }
                    else if (vKCode == 'Q')
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->leftShoulder, isDown);
                    }
                    else if (vKCode == 'E')
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->rightShoulder, isDown);
                    }
                    else if (vKCode == VK_UP)
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->actionUp, isDown);
                    }
                    else if (vKCode == VK_DOWN)
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->actionDown, isDown);
                    }
                    else if (vKCode == VK_LEFT)
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->actionLeft, isDown);
                    }
                    else if (vKCode == VK_RIGHT)
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->actionRight, isDown);
                    }
                    else if (vKCode == VK_ESCAPE)
                    {
                        running = false;
                    }
                    else if (vKCode == VK_SPACE)
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->start, isDown);
                    }
                    else if (vKCode == VK_BACK)
                    {
                        Win32ProcessKeyboardMessage(&keyboardController->back, isDown);
                    }

                    b32 altKeyWasDown = (message.lParam & (1 << 29));
                    if ((vKCode == VK_F4) && altKeyWasDown)
                    {
                        running = false;
                    }
                }
            } break;
            default:
            {
                TranslateMessage(&message);
                DispatchMessageA(&message);
            } break;
        }
    }
}

internal f32
Win32ProcessXInputStickValue(SHORT value, SHORT deadZoneThreshold)
{
    f32 result = 0;
    if (value < -deadZoneThreshold)
    {
        result = (f32)((value + deadZoneThreshold) / (32768.0f - deadZoneThreshold));
    }
    else if (value > deadZoneThreshold)
    {
        result = (f32)((value - deadZoneThreshold) / (32768.0f - deadZoneThreshold));
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

        #if HANDMADE_INTERNAL
            LPVOID baseAdress = (LPVOID)Terabytes(2);
        #else
            LPVOID baseAdress = 0;
        #endif

            HandmadeMemory handmadeMemory = {0};
            handmadeMemory.permanentStorageSize = Megabytes(64);
            handmadeMemory.transientStorageSize = Gigabytes(1);

            u64 totalStorageSize = handmadeMemory.permanentStorageSize + handmadeMemory.transientStorageSize;
            handmadeMemory.permanentStorage = VirtualAlloc(baseAdress, (size_t)totalStorageSize,
                                                           MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            handmadeMemory.transientStorage = ((u8 *)handmadeMemory.permanentStorage + handmadeMemory.permanentStorageSize);
            
            if (samples
                && handmadeMemory.permanentStorage
                && handmadeMemory.transientStorage)
            {

                // Since we specified CS_OWNDC, we can just
                // get one device context and use it forever because we
                // are not sharing it with anyone.
                HandmadeInput input[2] = {0};
                HandmadeInput *oldInput = &input[0];
                HandmadeInput *newInput = &input[1];
                LARGE_INTEGER lastCounter;
                QueryPerformanceCounter(&lastCounter);
                u64 lastCycleCount = __rdtsc();
                running = true;
                while (running)
                {
                    HandmadeControllerInput *oldKeyboardController = GetController(oldInput, 0);
                    HandmadeControllerInput *newKeyboardController = GetController(newInput, 0);
                    *newKeyboardController = (HandmadeControllerInput){0};
                    newKeyboardController->isConnected = true;
                    for(int buttonIndex = 0;
                        buttonIndex < ArrayCount(newKeyboardController->buttons);
                        buttonIndex++)
                    {
                        newKeyboardController->buttons[buttonIndex].endedDown =
                            oldKeyboardController->buttons[buttonIndex].endedDown;
                    }
                    Win32ProcessPendingMessages(newKeyboardController);

                    DWORD maxControllerCount = XUSER_MAX_COUNT;
                    if(maxControllerCount < (ArrayCount(newInput->controllers)-1))
                    {
                        maxControllerCount = (ArrayCount(newInput->controllers) - 1);
                    }

                    for (DWORD controllerIndex = 0; controllerIndex < maxControllerCount; controllerIndex++)
                    {
                        DWORD ourControllerIndex = controllerIndex+1;
                        HandmadeControllerInput *oldController = GetController(oldInput, ourControllerIndex);
                        HandmadeControllerInput *newController = GetController(newInput, ourControllerIndex);

                        XINPUT_STATE controllerState;
                        if (XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS)
                        {
                            newController->isConnected = true;
                            XINPUT_GAMEPAD *pad = &controllerState.Gamepad;
                            newController->isAnalog = true;

                            Win32ProcessXInputDigitalButton(pad->wButtons,
                                                            &oldController->actionDown,
                                                            XINPUT_GAMEPAD_A,
                                                            &newController->actionDown);

                            Win32ProcessXInputDigitalButton(pad->wButtons,
                                                            &oldController->actionRight,
                                                            XINPUT_GAMEPAD_B,
                                                            &newController->actionRight);

                            Win32ProcessXInputDigitalButton(pad->wButtons,
                                                            &oldController->actionLeft,
                                                            XINPUT_GAMEPAD_X,
                                                            &newController->actionLeft);

                            Win32ProcessXInputDigitalButton(pad->wButtons,
                                                            &oldController->actionUp,
                                                            XINPUT_GAMEPAD_Y,
                                                            &newController->actionUp);

                            Win32ProcessXInputDigitalButton(pad->wButtons,
                                                            &oldController->leftShoulder,
                                                            XINPUT_GAMEPAD_LEFT_SHOULDER,
                                                            &newController->leftShoulder);

                            Win32ProcessXInputDigitalButton(pad->wButtons,
                                                            &oldController->rightShoulder,
                                                            XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                                            &newController->rightShoulder);

                            Win32ProcessXInputDigitalButton(pad->wButtons,
                                                            &oldController->start,
                                                            XINPUT_GAMEPAD_START,
                                                            &newController->start);

                            Win32ProcessXInputDigitalButton(pad->wButtons,
                                                            &oldController->back,
                                                            XINPUT_GAMEPAD_BACK,
                                                            &newController->back);

                            newController->stickAverageX = Win32ProcessXInputStickValue(pad->sThumbLX,
                                                                XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

                            newController->stickAverageY = Win32ProcessXInputStickValue(pad->sThumbLY,
                                                                 XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                            if ((newController->stickAverageX != 0.0f) ||
                                (newController->stickAverageY != 0.0f))
                            {
                                newController->isAnalog = true;
                            }

                            if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                            {
                                newController->stickAverageY = 1.0f;
                                newController->isAnalog = false;
                            }
                            if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                            {
                                newController->stickAverageY = -1.0f;
                                newController->isAnalog = false;
                            }
                            if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                            {
                                newController->stickAverageX = -1.0f;
                                newController->isAnalog = false;
                            }
                            if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                            {
                                newController->stickAverageX = 1.0f;
                                newController->isAnalog = false;
                            }

                            f32 threshold = 0.5f;

                            Win32ProcessXInputDigitalButton((newController->stickAverageX > threshold) ? 1 : 0,
                                                            &oldController->moveRight, 1,
                                                            &newController->moveRight);
                            Win32ProcessXInputDigitalButton((newController->stickAverageX < -threshold) ? 1 : 0,
                                                            &oldController->moveLeft, 1,
                                                            &newController->moveLeft);
                            Win32ProcessXInputDigitalButton((newController->stickAverageY > threshold) ? 1 : 0,
                                                            &oldController->moveUp, 1,
                                                            &newController->moveUp);
                            Win32ProcessXInputDigitalButton((newController->stickAverageY < -threshold) ? 1 : 0,
                                                            &oldController->moveDown, 1,
                                                            &newController->moveDown);

                        }
                        else
                        {
                            newController->isConnected = false;
                        }
                    }

                    DWORD byteToLock = 0;
                    DWORD bytesToWrite = 0;
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
                            // Play cuysor is in front
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

                    HandmadeUpdateAndRender(&handmadeMemory, newInput, &buffer, &soundBuffer);

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
                    f32 msPerFrame = 1000.0f * (f32)counterElapsed / (f32)perfCountFrequency;
                    f32 fps = (f32)perfCountFrequency / (f32)counterElapsed;
                    f32 megaCyclesPerFrame = (f32)cyclesElapsed / (1000.0f * 1000.0f);
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
                // Memory allocation failed
            }
        }
        else
        {
            // Window creation failed
        }
    }
    else
    {
        // Window class registration failed
    }

    return 0;
}
