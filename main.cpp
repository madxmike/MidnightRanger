
#include "SDL3/SDL_filesystem.h"
#include "SDL3/SDL_gpu.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_iostream.h"
#include "SDL3/SDL_log.h"
#include "SDL3/SDL_pixels.h"
#include "SDL3/SDL_stdinc.h"
#include "SDL3_shadercross/SDL_shadercross.h"
#include <iostream>
#include <string>

constexpr SDL_FColor CLEAR_COLOR{
    .r = 0.0,
    .g = 0.0,
    .b = 0.0,
    .a = 1.0,
};

struct SimpleVertex {
  float x;
  float y;
  float z;
  float r;
  float g;
  float b;
};

SDL_GPUShader *LoadAndCompileShader(SDL_GPUDevice *device,
                                    const std::string shaderName) {
  const std::string basePath = SDL_GetBasePath();
  const std::string shaderFilePath =
      basePath + "Content/Shaders/" + shaderName + ".hlsl";

  SDL_ShaderCross_ShaderStage stage;
  if (shaderName.find(".vert") != std::string::npos) {
    stage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX;
  } else if (shaderName.find(".frag") != std::string::npos) {
    stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
  } else {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                 "Could not determine shader stage for shader: %s\n",
                 shaderName.c_str());
    return nullptr;
  }

  std::cout << shaderName << stage << std::endl;

  size_t codeSize;
  void *shaderSource = SDL_LoadFile(shaderFilePath.c_str(), &codeSize);
  if (shaderSource == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to load file from disk: %s\n",
                 shaderFilePath.c_str());
    SDL_free(shaderSource);
    return nullptr;
  }
  SDL_ShaderCross_HLSL_Info shaderHLSLInfo = {
      .source = (char *)shaderSource,
      .entrypoint = "main",
      .include_dir = nullptr,
      .defines = nullptr,
      .shader_stage = stage,
  };

  size_t sprivBytecodeSize;
  Uint8 *spirvBytecode = (Uint8 *)SDL_ShaderCross_CompileSPIRVFromHLSL(
      &shaderHLSLInfo, &sprivBytecodeSize);
  if (spirvBytecode == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not compile %s to SPIRV: %s\n",
                 shaderName.c_str(), SDL_GetError());
    SDL_free(spirvBytecode);
    return nullptr;
  }
  SDL_ShaderCross_SPIRV_Info shaderSpirvInfo = SDL_ShaderCross_SPIRV_Info{
      .bytecode = spirvBytecode,
      .bytecode_size = sprivBytecodeSize,
      .entrypoint = "main",
      .shader_stage = stage,
      .props = 0,
  };

  SDL_ShaderCross_GraphicsShaderMetadata *spirvMetadata =
      SDL_ShaderCross_ReflectGraphicsSPIRV(spirvBytecode, sprivBytecodeSize, 0);
  if (spirvMetadata == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                 "Could not reflect SPIRV metadata for %s: %s\n",
                 shaderName.c_str(), SDL_GetError());
    SDL_free(spirvBytecode);
    return nullptr;
  }
  SDL_GPUShader *shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
      device, &shaderSpirvInfo, &spirvMetadata->resource_info, 0);
  if (shader == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not compile shader %s: %s\n",
                 shaderName.c_str(), SDL_GetError());
    return nullptr;
  }
  SDL_free(shaderSource);
  SDL_free(spirvBytecode);

  return shader;
}

int main() {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
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

  SDL_GPUColorTargetDescription colorTargetDescription = {
      .format = SDL_GetGPUSwapchainTextureFormat(device, window)};

  SDL_GPUVertexBufferDescription vertexBufferDescriptions[] = {
      {
          .slot = 0,
          .pitch = sizeof(SimpleVertex),
          .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
          .instance_step_rate = 0
      }
  };

  SDL_GPUVertexAttribute vertexAttributes[] = {
      {
          .location = 0,
          .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
          .offset = 0,
      },
      {
          .location = 1,
          .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
          .offset = sizeof(float) * 3,
      }};
  SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
      .vertex_shader = LoadAndCompileShader(device, "simple.vert"),
      .fragment_shader = LoadAndCompileShader(device, "simple.frag"),
      .vertex_input_state =
          {
              .vertex_buffer_descriptions = vertexBufferDescriptions,
              .num_vertex_buffers = 1,
              .vertex_attributes = vertexAttributes,
              .num_vertex_attributes = 2,
          },
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
      .target_info =
          {
              .color_target_descriptions = &colorTargetDescription,
              .num_color_targets = 1,
          },
  };

  pipelineCreateInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;

  SDL_GPUGraphicsPipeline *pipeline =
      SDL_CreateGPUGraphicsPipeline(device, &pipelineCreateInfo);

  bool continuePlay = true;
  while (continuePlay) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        continuePlay = false;
      }
    }

    SDL_GPUCommandBuffer *commandBuffer = SDL_AcquireGPUCommandBuffer(device);
    if (commandBuffer == nullptr) {
      SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                   "Could not acquire command buffer: %s\n", SDL_GetError());
      return -1;
    }

    SDL_GPUTexture *swapchainTexture;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            commandBuffer, window, &swapchainTexture, nullptr, nullptr)) {
      SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                   "Could not acquire swapchain texture: %s\n", SDL_GetError());
      return -1;
    }

    SDL_GPUBufferCreateInfo bufferCreateInfo = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX |
                 SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
        .size = sizeof(SimpleVertex) * 3,
        .props = 0,
    };

    SDL_GPUBuffer *vertexBuffer =
        SDL_CreateGPUBuffer(device, &bufferCreateInfo);
    if (vertexBuffer == nullptr) {
      SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                   "Could not acquire vertex buffer: %s\n", SDL_GetError());
      return -1;
    }

    SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = sizeof(SimpleVertex) * 3,
    };

    SDL_GPUTransferBuffer *transferBuffer =
        SDL_CreateGPUTransferBuffer(device, &transferBufferCreateInfo);

    void *mappedTransferBuffer =
        SDL_MapGPUTransferBuffer(device, transferBuffer, true);
    if (mappedTransferBuffer == nullptr) {
      SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                   "Could not acquire map transfer buffer: %s\n",
                   SDL_GetError());
      return -1;
    }

    SimpleVertex *vertexData = (SimpleVertex *)mappedTransferBuffer;
    vertexData[0] = (SimpleVertex){
        .x = 0.0f, .y = 0.75f, .z = 0.0f, .r = 1.0f, .g = 0.0f, .b = 0.0f};
    vertexData[1] = (SimpleVertex){
        .x = 0.75f, .y = -0.75f, .z = 0.0f, .r = 0.0f, .g = 1.0f, .b = 0.0f};
    vertexData[2] = (SimpleVertex){
        .x = -0.75f, .y = -0.75f, .z = 0.0f, .r = 0.0f, .g = 0.0f, .b = 1.0f};

    SDL_UnmapGPUTransferBuffer(device, transferBuffer);

    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(commandBuffer);

    SDL_GPUTransferBufferLocation transferBufferLocation = {
        .transfer_buffer = transferBuffer,
        .offset = 0,
    };

    SDL_GPUBufferRegion bufferRegion = {
        .buffer = vertexBuffer, .offset = 0, .size = sizeof(SimpleVertex) * 3};

    SDL_UploadToGPUBuffer(copyPass, &transferBufferLocation, &bufferRegion,
                          false);
    SDL_EndGPUCopyPass(copyPass);

    SDL_GPUColorTargetInfo colorTargetInfo = {
        .texture = swapchainTexture,
        .mip_level = 0,
        .clear_color = CLEAR_COLOR,
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
        .cycle = true,
    };

    SDL_GPURenderPass *renderPass =
        SDL_BeginGPURenderPass(commandBuffer, &colorTargetInfo, 1, nullptr);
    if (renderPass == nullptr) {
      SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not bein render pass: %s\n",
                   SDL_GetError());
      return -1;
    }

    SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

    SDL_GPUBufferBinding bufferBinding = {
        .buffer = vertexBuffer,
        .offset = 0,
    };
    SDL_BindGPUVertexBuffers(renderPass, 0, &bufferBinding, 1);

    // SDL_BindGPUVertexStorageBuffers(renderPass, 0, &vertexBuffer, 1);

    SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);

    SDL_EndGPURenderPass(renderPass);
    SDL_SubmitGPUCommandBuffer(commandBuffer);
  }

  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
