#include "pipelines.h"
#include "SDL3/SDL_gpu.h"
#include "SDL3_shadercross/SDL_shadercross.h"
#include <string>

SDL_GPUShader* LoadAndCompileShader(SDL_GPUDevice *device,
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
      .entrypoint = "Main",
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
      .entrypoint = "Main",
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

SDL_GPUGraphicsPipeline* LoadSpritePipeline(SDL_GPUDevice* device, SDL_GPUTextureFormat textureFormat) {
    SDL_GPUVertexBufferDescription vertexBufferDescriptions[] = {
        {
            .slot = 0,
            .pitch = sizeof(SpriteVertex),
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
        },
        {
            .location = 2,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
            .offset = sizeof(float) * 6,
        }
    };

    SDL_GPUColorTargetDescription colorTargetDescription = {
        .format = textureFormat
    };

    SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .vertex_shader = LoadAndCompileShader(device, "Sprite.vert"),
        .fragment_shader = LoadAndCompileShader(device, "Sprite.frag"),
        .vertex_input_state =
            {
                .vertex_buffer_descriptions = vertexBufferDescriptions,
                .num_vertex_buffers = 1,
                .vertex_attributes = vertexAttributes,
                .num_vertex_attributes = 3,
            },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .target_info =
            {
                .color_target_descriptions = &colorTargetDescription,
                .num_color_targets = 1,
            },

    };

    SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineCreateInfo);

    SDL_ReleaseGPUShader(device, pipelineCreateInfo.vertex_shader);
    SDL_ReleaseGPUShader(device, pipelineCreateInfo.fragment_shader);

    return pipeline;
}

GraphicsPipelines LoadPipelines(SDL_GPUDevice* device, SDL_Window* window) {
    SDL_GPUTextureFormat textureFormat = SDL_GetGPUSwapchainTextureFormat(device, window);

    return {
        .sprite = LoadSpritePipeline(device, textureFormat),
    };
}
