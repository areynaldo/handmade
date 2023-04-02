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

internal void
HandmadeOutputSound(HandmadeSoundOutputBuffer *soundBuffer, int toneHz)
{
    persist f32 tSine;
    i16 toneVolume = 3000;
    int wavePeriod = soundBuffer->samplesPerSecond / toneHz;
    i16 *sampleOut = soundBuffer->samples;
    for (int sampleIndex = 0; sampleIndex < soundBuffer->sampleCount; sampleIndex++)
    {
        f32 sineValue = sinf(tSine);
        i16 sampleValue = (i16)(sineValue * toneVolume);

        *sampleOut++ = sampleValue;
        *sampleOut++ = sampleValue;

        tSine += (2.0f * Pi32 * 1.0f) / (f32)wavePeriod;
    }
}

void HandmadeUpdateAndRender(HandmadeInput *input, HandmadeOffscreenBuffer *buffer,
                             HandmadeSoundOutputBuffer *soundBuffer)
{
    persist int xOffset;
    persist int yOffset;
    persist int toneHz;

    HandmadeControllerInput *input0 = &input->controllers[0];
    if (input0->isAnalog)
    {
        xOffset += (int)(4.0f * input0->endX);
        toneHz = 256 + (int)(128.0f * (input0->endY));
    }
    else
    {

    }

    if(input0->down.endedDown)
    {
        yOffset += 1;
    }

    HandmadeOutputSound(soundBuffer, toneHz);
    RenderWeirdGradient(buffer, xOffset, yOffset);
}