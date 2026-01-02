
#include "SDL3/SDL_stdinc.h"
#include "SDL3/SDL_surface.h"
#include "SDL3_shadercross/SDL_shadercross.h"
#include "SDL3_image/SDL_image.h"
#include "draw.h"
#include "pipelines.h"
#include "SDL3/SDL_gpu.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_log.h"
#include "SDL3/SDL_video.h"
#include <string>

constexpr int WindowWidth = 800;
constexpr int WindowHeight = 600;

struct SDL_Context {
    SDL_Window* window;
    SDL_GPUDevice* device;
};

SDL_Context initSDL() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
      SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not init sdl: %s\n",
                   SDL_GetError());
      return {};
    }

    if (!SDL_ShaderCross_Init()) {
      SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not init shadercross: %s\n",
                   SDL_GetError());
      return {};
    }

    SDL_Window *window = SDL_CreateWindow("Test", WindowWidth, WindowHeight, SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
      SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create window: %s\n",
                   SDL_GetError());
      return {};
    }

    SDL_GPUDevice *device =
        SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
    if (device == nullptr) {
      SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create gpu device: %s\n",
                   SDL_GetError());
      return {};
    }

    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
      SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                   "Could not claim window for gpu device: %s\n", SDL_GetError());
      return {};
    }

    return {
        .window = window,
        .device = device,
    };
}

SDL_GPUTexture* LoadAndCreateTexture(SDL_GPUDevice *device, std::string fileName) {
    const std::string basePath = SDL_GetBasePath();
    const std::string filePath = basePath + "Content/Images/" + fileName;

    SDL_Surface* surface = IMG_Load(filePath.c_str());
    if (surface == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                     "Could not load texture %s: %s\n", filePath.c_str(),  SDL_GetError());
        return nullptr;
    }

    const Uint32 textureWidth = static_cast<Uint32>(surface->w);
    const Uint32 textureHeight = static_cast<Uint32>(surface->h);

    SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = textureWidth * textureHeight * 4,
    };
    SDL_GPUTransferBuffer *transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferBufferCreateInfo);
    {
        void *buffer = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
        SDL_memcpy(buffer, surface->pixels, textureWidth * textureHeight * 4);
        SDL_UnmapGPUTransferBuffer(device, transferBuffer);
    }

    SDL_DestroySurface(surface);

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

    SDL_GPUCommandBuffer *commandBuffer = SDL_AcquireGPUCommandBuffer(device);
    if (commandBuffer == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                    "Could not acquire command buffer: %s\n", SDL_GetError());
        return nullptr;
    }

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);
    if (copyPass == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                    "Could not acquire copy pass: %s\n", SDL_GetError());
        return nullptr;
    }

    SDL_GPUTextureTransferInfo transferBufferLocation = {
        .transfer_buffer = transferBuffer,
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
    SDL_ReleaseGPUTransferBuffer(device, transferBuffer);

    return texture;
}

int main() {

    SDL_Context sdlContext = initSDL();

    SDL_GPUTexture *texture = LoadAndCreateTexture(sdlContext.device, "test_sprite.png");

  SDL_GPUSamplerCreateInfo samplerCreateInfo = {
      .min_filter = SDL_GPU_FILTER_NEAREST,
      .mag_filter = SDL_GPU_FILTER_NEAREST,
      .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
      .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
      .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
  };
  SDL_GPUSampler *sampler = SDL_CreateGPUSampler(sdlContext.device, &samplerCreateInfo);

  GraphicsPipelines pipelines = LoadPipelines(sdlContext.device, sdlContext.window);

  bool continuePlay = true;
  while (continuePlay) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        continuePlay = false;
      }
    }

    DrawSprite(sdlContext.device, sdlContext.window, pipelines.sprite, texture, sampler);
  }

  SDL_ReleaseGPUTexture(sdlContext.device, texture);
  SDL_ReleaseGPUSampler(sdlContext.device, sampler);
  SDL_DestroyWindow(sdlContext.window);
  SDL_Quit();
  return 0;
}
