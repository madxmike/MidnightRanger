
#include "SDL3/SDL_stdinc.h"
#include "SDL3/SDL_surface.h"
#include "SDL3_shadercross/SDL_shadercross.h"
#include "SDL3_image/SDL_image.h"
#include "pipelines.h"
#include "draw.h"
#include "SDL3/SDL_gpu.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_log.h"
#include "SDL3/SDL_video.h"
#include <iostream>
#include <ostream>
#include <string>

int main() {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS  )) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not init sdl: %s\n",
                 SDL_GetError());
    return -1;
  }

  if (!SDL_ShaderCross_Init()) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not init shadercross: %s\n",
                 SDL_GetError());
    return -1;
  }

  SDL_Window *window = SDL_CreateWindow("Test", 800, 600, SDL_WINDOW_RESIZABLE);
  if (window == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create window: %s\n",
                 SDL_GetError());
    return -1;
  }

  SDL_GPUDevice *device =
      SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
  if (device == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create gpu device: %s\n",
                 SDL_GetError());
    return -1;
  }

  if (!SDL_ClaimWindowForGPUDevice(device, window)) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                 "Could not claim window for gpu device: %s\n", SDL_GetError());
    return -1;
  }


  const std::string basePath = SDL_GetBasePath();
  const std::string filePath = basePath + "Content/Images/" + "test-sprite.png";

  std::cout << "Loading image " << filePath << std::endl;
  SDL_Surface* surface = IMG_Load(filePath.c_str());
  if (surface == nullptr) {
      SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                   "Could not load surface: %s\n", SDL_GetError());
      return -1;
  }
  const Uint32 textureWidth = static_cast<Uint32>(surface->w);
  const Uint32 textureHeight = static_cast<Uint32>(surface->h);

  SDL_GPUTextureCreateInfo textureCreateInfo = {
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
      .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
      .width = textureWidth,
      .height = textureWidth,
      .layer_count_or_depth = 1,
      .num_levels = 1,
  };

  SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &textureCreateInfo);

  SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo = {
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = textureWidth * textureHeight * 4,
  };

  SDL_GPUTransferBuffer *textureTransferBuffer = SDL_CreateGPUTransferBuffer(device, &transferBufferCreateInfo);

  {
      SDL_GPUCommandBuffer *commandBuffer = SDL_AcquireGPUCommandBuffer(device);
      if (commandBuffer == nullptr) {
          SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                      "Could not acquire copy pass command buffer: %s\n", SDL_GetError());
          return -1;
      }

      void *buffer = SDL_MapGPUTransferBuffer(device, textureTransferBuffer, false);
      SDL_memcpy(buffer, surface->pixels, textureWidth * textureHeight * 4);
      SDL_UnmapGPUTransferBuffer(device, textureTransferBuffer);

      SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);
      if (copyPass == nullptr) {
          SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                      "Could not acquire copy pass: %s\n", SDL_GetError());
          return -1;
      }
      SDL_GPUTextureTransferInfo transferBufferLocation = {
          .transfer_buffer = textureTransferBuffer,
          .offset = 0,
      };
      SDL_GPUTextureRegion textureRegion = {
          .texture = texture,
          .w = textureWidth,
          .h = textureHeight,
          .d = 1,
      };

      SDL_UploadToGPUTexture(copyPass, &transferBufferLocation, &textureRegion, false);
      SDL_EndGPUCopyPass(copyPass);
      SDL_SubmitGPUCommandBuffer(commandBuffer);

      SDL_ReleaseGPUTransferBuffer(device, textureTransferBuffer);
      SDL_DestroySurface(surface);
  }

  SDL_GPUSamplerCreateInfo samplerCreateInfo = {
      .min_filter = SDL_GPU_FILTER_NEAREST,
      .mag_filter = SDL_GPU_FILTER_NEAREST,
      .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
      .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
  };
  SDL_GPUSampler *sampler = SDL_CreateGPUSampler(device, &samplerCreateInfo);

  GraphicsPipelines pipelines = LoadPipelines(device, window);

  bool continuePlay = true;
  while (continuePlay) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        continuePlay = false;
      }
    }

    DrawSprite(device, window, pipelines.sprite, texture, sampler);
  }

  SDL_ReleaseGPUTexture(device, texture);
  SDL_ReleaseGPUSampler(device, sampler);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
