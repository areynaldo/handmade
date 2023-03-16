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

global bool running;
global void *bitmapMemory;
global int bitmapWidth;
global int bitmapHeight;
global int bytesPerPixel;

internal void 
RenderWeirdGradient(int xOffset, int yOffset)
{
    int pitch = bitmapWidth * bytesPerPixel;

    u8 *row = (u8 *)bitmapMemory;
    for(int y = 0; y < bitmapHeight; y++)
    {
        u32 *pixel = (u32 *)row;
        for(int x = 0; x < bitmapWidth; x++)
        {
            // Pixel Memory BB GG RR XX
            u8 red = (u8)(x + yOffset);
            u8 green = (u8)(y + yOffset);
            u8 blue = (u8)(x + xOffset);

            *pixel++ = red << 16 | green << 8 | blue;
        }
        row += pitch;
    }
}

internal void
Win32ResizeDIBSection(int width, int height)
{

    if(bitmapMemory)
    {
        VirtualFree(bitmapMemory, 0, MEM_RELEASE);
    }

    bitmapWidth = width;
    bitmapHeight = height;
    bytesPerPixel = 4;
    int bitmapMemorySize = bytesPerPixel * (width * height);

    bitmapMemory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

}

internal void
Win32UpdateWindow(HDC deviceContext, RECT *clientRect)
{
    BITMAPINFO bitmapInfo = {0};

    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = bitmapWidth;
    bitmapInfo.bmiHeader.biHeight = -bitmapHeight; // negative value to down pitch
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    int windowWidth = clientRect->right - clientRect->left;
    int windowHeight = clientRect->bottom - clientRect->top;
    StretchDIBits(deviceContext,
                  0, 0, windowWidth, windowHeight,
                  0, 0, bitmapWidth, bitmapHeight,
                  bitmapMemory,
                  &bitmapInfo,
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
        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint(window, &paint);
            RECT clientRect;
            GetClientRect(window, &clientRect);
            Win32UpdateWindow(deviceContext, &clientRect);
            EndPaint(window, &paint);
        } 
        break;

        case WM_SIZE:
        {
            RECT clientRect;
            GetClientRect(window, &clientRect);
            int width = clientRect.right - clientRect.left;
            int height = clientRect.bottom - clientRect.top;
            Win32ResizeDIBSection(width, height);
            OutputDebugStringA("WM_SIZE\n");
        } 
        break;

        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
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
            running = true;
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
                RenderWeirdGradient(xOffset, yOffset);
                xOffset++;
                yOffset++;

                HDC deviceContext = GetDC(window);
                RECT clientRect;
                GetClientRect(window, &clientRect);
                Win32UpdateWindow(deviceContext, &clientRect);
                ReleaseDC(window, deviceContext);
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
