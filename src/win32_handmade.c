#include <windows.h> 
#include <stdbool.h>
#include <stdint.h>

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

typedef struct {
    int width;
    int height;
} Win32WindowDimension;

typedef struct {
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

internal Win32WindowDimension  Win32GetWindowDimension(HWND window)
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
    for(int y = 0; y < buffer->height; y++)
    {
        u32 *pixel = (u32 *)row;
        for(int x = 0; x < buffer->width; x++)
        {
            // Pixel Memory BB GG RR XX
            u8 red = (u8)(x + yOffset);
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

    if(buffer->memory)
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
    switch(message)
    {
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
        LPSTR     commandLine,
        int       showCommandLine)
{
    WNDCLASSA windowClass = {0};
    windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = Win32MainWindowCallback;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = "HandmadeWindowClass";

    if(RegisterClassA(&windowClass))
    {
        HWND window = CreateWindowExA(0, windowClass.lpszClassName, "Handmade",
                                      WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                      CW_USEDEFAULT, CW_USEDEFAULT,
                                      CW_USEDEFAULT, CW_USEDEFAULT,
                                      0, 0, instance, 0);
        if(window)
        {
            Win32ResizeDIBSection(&globalBackBuffer, 1280, 720); 
            running = true;
            // Since we specified CS_OWNDC, we can just
            // get one device context and use it forever because we
            // are not sharing it with anyone. 
            HDC  deviceContext = GetDC(window);
            int xOffset = 0;
            int yOffset = 0;
            while(running)
            {
                MSG message;
                while(PeekMessageA(&message, 0, 0, 0, PM_REMOVE))
                {
                    if(message.message == WM_QUIT)
                    {
                        running = false;
                    }

                    TranslateMessage(&message);
                    DispatchMessageA(&message);
                }
                RenderWeirdGradient(&globalBackBuffer, xOffset, yOffset);
                xOffset++;
                yOffset++;
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
