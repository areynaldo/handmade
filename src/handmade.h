#ifndef HANDMADE_H
#define HANDMADE_H

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

void HandmadeUpdateAndRender(HandmadeOffscreenBuffer *buffer, int xOffset, int yOffset,
                             HandmadeSoundOutputBuffer *soundBuffer, int toneHz);

#endif