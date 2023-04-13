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
            u8 blue = (u8)(x - xOffset);

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

void
HandmadeUpdateAndRender(HandmadeMemory *memory, HandmadeInput *input,
                        HandmadeOffscreenBuffer *buffer, HandmadeSoundOutputBuffer *soundBuffer)
{
    Assert((&input->controllers[0].terminator - &input->controllers[0].buttons[0]) ==
            (ArrayCount(input->controllers[0].buttons)));

    DebugReadFileResult fileData = DEBUGPlatformReadEntireFile(__FILE__);
    if (fileData.contents)
    {
        DEBUGPlatformWriteEntireFile("test.out", fileData.contentsSize, fileData.contents);
        DEBUGPlatformFreeFileMemory(fileData.contents);
    }

    Assert(sizeof(HandmadeState) <= memory->permanentStorageSize);
    HandmadeState *handmadeState = (HandmadeState *)memory->permanentStorage;

    if (!memory->isInitialized)
    {
        handmadeState->xOffset = 0;
        handmadeState->yOffset = 0;
        handmadeState->toneHz = 256;

        memory->isInitialized = true;
    }

    for (int controllerIndex = 0;
         controllerIndex < ArrayCount(input->controllers);
         controllerIndex++)
    {
        HandmadeControllerInput *controller = GetController(input, controllerIndex);
        if (controller->isAnalog)
        {
            handmadeState->toneHz = 256 + (int)(128.0f * (controller->stickAverageY));
            handmadeState->xOffset += (int)(4.0f * controller->stickAverageX);
            handmadeState->yOffset += (int)(4.0f * controller->stickAverageY);
        }
        else
        {
            if (controller->moveLeft.endedDown)
            {
                handmadeState->xOffset -= 1;

            }

            if (controller->moveRight.endedDown)
            {
                handmadeState->xOffset += 1;
            }
        }

        if(controller->actionDown.endedDown)
        {
            handmadeState->yOffset -= 1;
        }
    }

    HandmadeOutputSound(soundBuffer, handmadeState->toneHz);
    RenderWeirdGradient(buffer, handmadeState->xOffset, handmadeState->yOffset);
}