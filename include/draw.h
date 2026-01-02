#pragma once

#include "SDL3/SDL_gpu.h"

void DrawSprite(SDL_GPUDevice *device, SDL_Window* window, SDL_GPUGraphicsPipeline *pipeline, SDL_GPUTexture *texture, SDL_GPUSampler *sampler);
