#include "handmade.h"

internal void
RenderWeirdGradient(HandmadeOffscreenBuffer *buffer, int xOffset, int yOffset)
{
    u8 *row = (u8 *)buffer->memory;
    for (int y = 0; y < buffer->height; y++)
    {
        u32 *pixel = (u32 *)row;
        for (int x = 0; x < buffer->width; x++)
        {
            // Pixel Memory BB GG RR XX
            u8 red = 0;
            u8 green = (u8)(y + yOffset);
            u8 blue = (u8)(x + xOffset);

            *pixel++ = red << 16 | green << 8 | blue;
        }
        row += buffer->pitch;
    }
}

void
HandmadeUpdateAndRender(HandmadeOffscreenBuffer *buffer)
{
    int xOffset = 0;
    int yOffset = 0;

    RenderWeirdGradient(buffer, xOffset, yOffset);
}