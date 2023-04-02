#ifndef WIN32_HANDMADE_H
#define WIN32_HANDMADE_H

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

typedef struct
{
    int samplesPerSecond;
    int bytesPerSample;
    int secondaryBufferSize;
    u32 runningSampleIndex;
    int latencySampleCount;
} Win32SoundOutput;

#endif