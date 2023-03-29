#ifndef HANDMADE_H
#define HANDMADE_H

typedef struct
{
    void *memory;
    int width;
    int height;
    int pitch;
} HandmadeOffscreenBuffer;

void HandmadeUpdateAndRender(HandmadeOffscreenBuffer *buffer);

#endif