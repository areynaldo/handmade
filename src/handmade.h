#if HANDMADE_SLOW
#define Assert(expression) if (!(expression)) { *(int *)0 = 0; }
#else
#define Assert(expression)
#endif

#ifndef HANDMADE_H
#define HANDMADE_H
#define Kilobytes(value) ((value)*1024LL)
#define Megabytes(value) (Kilobytes(value)*1024LL)
#define Gigabytes(value) (Megabytes(value)*1024LL)
#define Terabytes(value) (Gigabytes(value)*1024LL)

#define ArrayCount(array) (sizeof(array)/ sizeof((array)[0]))

typedef struct
{
    void *memory;
    int width;
    int height;
    int pitch;
} HandmadeOffscreenBuffer;

typedef struct
{
    int samplesPerSecond;
    int sampleCount;
    i16 *samples;
} HandmadeSoundOutputBuffer;

typedef struct
{
    i32 halfTransitionCount;
    b32 endedDown;
} HandmadeButtonState;

typedef struct
{
    b32 isAnalog;

    f32 startX;
    f32 startY;

    f32 minX;
    f32 minY;

    f32 maxX;
    f32 maxY;

    f32 endX;
    f32 endY;

    union {
        HandmadeButtonState buttons[6];
        struct 
        {
            HandmadeButtonState up;
            HandmadeButtonState down;
            HandmadeButtonState left;
            HandmadeButtonState right;
            HandmadeButtonState leftShoulder;
            HandmadeButtonState rightShoulder;
        };
    };
} HandmadeControllerInput;

typedef struct
{
    u64 permanentStorageSize;
    void *permanentStorage;
    u64 transientStorageSize;
    void *transientStorage;

    b32 isInitialized;
} HandmadeMemory;

typedef struct
{
    HandmadeControllerInput controllers[4];
} HandmadeInput;

void HandmadeUpdateAndRender(HandmadeMemory *memory, HandmadeInput *input,
                             HandmadeOffscreenBuffer *buffer, HandmadeSoundOutputBuffer *soundBuffer);

typedef struct
{
    int toneHz;
    int xOffset;
    int yOffset;
} HandmadeState;

#endif