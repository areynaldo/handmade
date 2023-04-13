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

inline u32
SafeTruncateUInt64(u64 value)
{
    Assert(value <= 0xFFFFFFFF);
    u32 result = (u32)value;
    return result;
}

// Services the platform layer provides to the app

#if HANDMADE_INTERNAL
typedef struct
{
    u32 contentsSize;
    void *contents;
} DebugReadFileResult;
internal DebugReadFileResult DEBUGPlatformReadEntireFile(char *fileName);
internal void DEBUGPlatformFreeFileMemory(void *memory);
internal b32 DEBUGPlatformWriteEntireFile(char *fileName, u32 memorySize, void *memory);
# endif

// Services the app provides to the plattform layer
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
    b32 isConnected;
    b32 isAnalog;

    f32 stickAverageX;
    f32 stickAverageY;

    union {
        HandmadeButtonState buttons[12];
        struct 
        {
            HandmadeButtonState moveUp;
            HandmadeButtonState moveDown;
            HandmadeButtonState moveLeft;
            HandmadeButtonState moveRight;

            HandmadeButtonState actionUp;
            HandmadeButtonState actionDown;
            HandmadeButtonState actionLeft;
            HandmadeButtonState actionRight;


            HandmadeButtonState leftShoulder;
            HandmadeButtonState rightShoulder;
            HandmadeButtonState back;
            HandmadeButtonState start;

            HandmadeButtonState terminator;
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
    HandmadeControllerInput controllers[5];
} HandmadeInput;

inline HandmadeControllerInput *GetController(HandmadeInput *input, int controllerIndex)
{
    Assert(controllerIndex < ArrayCount(input->controllers));
    HandmadeControllerInput *result = &input->controllers[controllerIndex];
    return result;
}

void HandmadeUpdateAndRender(HandmadeMemory *memory, HandmadeInput *input,
                             HandmadeOffscreenBuffer *buffer, HandmadeSoundOutputBuffer *soundBuffer);

typedef struct
{
    int toneHz;
    int xOffset;
    int yOffset;
} HandmadeState;

#endif