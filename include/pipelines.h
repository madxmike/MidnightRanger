#pragma once

#include "SDL3/SDL_gpu.h"

struct SpriteVertex {
    float x, y, z;
    float r, g, b;
    float u, v;
};

struct GraphicsPipelines {
    SDL_GPUGraphicsPipeline* sprite;
};

GraphicsPipelines LoadPipelines(SDL_GPUDevice* device, SDL_Window* window);
