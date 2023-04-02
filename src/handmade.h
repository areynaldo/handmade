#ifndef HANDMADE_H
#define HANDMADE_H

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
    HandmadeControllerInput *controllers[4];
} HandmadeInput;

void HandmadeUpdateAndRender(HandmadeInput *input, HandmadeOffscreenBuffer *buffer,
                             HandmadeSoundOutputBuffer *soundBuffer);


#endif