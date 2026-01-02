#include "SDL3/SDL_gpu.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_log.h"
#include "SDL3/SDL_video.h"
#include "SDL3_image/SDL_image.h"
#include "SDL3_shadercross/SDL_shadercross.h"
#include "glm/fwd.hpp"
#include <vector>
#include "rendering.h"

namespace rendering {
    namespace {
        struct SpriteVertex {
            float x, y, z;
            float r, g, b;
            float u, v;
        };

        struct GraphicsPipelines {
            SDL_GPUGraphicsPipeline* sprite;
        };


        struct Samplers {
            SDL_GPUSampler *nearest_clamped;
        };

        constexpr int WindowWidth = 800;
        constexpr int WindowHeight = 600;

        constexpr SDL_FColor CLEAR_COLOR{
            .r = 0.0,
            .g = 0.0,
            .b = 0.0,
            .a = 1.0,
        };

        SDL_GPUDevice *device;
        SDL_Window *window;
        Samplers samplers;
        GraphicsPipelines pipelines;

        // Note: Append only to keep handles consistent for the lifetime of the game
        std::vector<SDL_GPUTexture *> textures;

        // TODO (Michael): Use batching instead https://moonside.games/posts/sdl-gpu-sprite-batcher/
        SDL_GPUBuffer *spriteVertexBuffer;
        SDL_GPUBuffer *spriteIndexBuffer;
        SDL_GPUTransferBuffer *spriteTransferBuffer;
    }

    SDL_GPUShader* LoadAndCompileShader(SDL_GPUDevice *device, const std::string shaderName) {
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

    Samplers InitSamplers() {
        SDL_GPUSamplerCreateInfo nearestClampedSamplerCreateInfo = {
            .min_filter = SDL_GPU_FILTER_NEAREST,
            .mag_filter = SDL_GPU_FILTER_NEAREST,
            .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
            .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        };
        return {
            .nearest_clamped = SDL_CreateGPUSampler(device, &nearestClampedSamplerCreateInfo)
        };
    }

    void InitRenderer() {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
          SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not init sdl video: %s\n", SDL_GetError());
          return;
        }

        if (!SDL_ShaderCross_Init()) {
          SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not init shadercross: %s\n",  SDL_GetError());
          return;
        }

        window = SDL_CreateWindow("Test", WindowWidth, WindowHeight, SDL_WINDOW_RESIZABLE);
        if (window == nullptr) {
          SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create window: %s\n", SDL_GetError());
          return;
        }

        device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
        if (device == nullptr) {
          SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create gpu device: %s\n", SDL_GetError());
          return;
        }

        if (!SDL_ClaimWindowForGPUDevice(device, window)) {
          SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not claim window for gpu device: %s\n", SDL_GetError());
          return;
        }

        samplers = InitSamplers();

        SDL_GPUTextureFormat textureFormat = SDL_GetGPUSwapchainTextureFormat(device, window);

        pipelines = {
            .sprite = LoadSpritePipeline(device, textureFormat),
        };
    }

    void ReleaseResources() {
        for (const auto& texture : textures) {
            SDL_ReleaseGPUTexture(device, texture);
        }

        SDL_ReleaseGPUSampler(device, samplers.nearest_clamped);
        SDL_DestroyGPUDevice(device);
        SDL_DestroyWindow(window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    TextureHandle LoadAndRegisterTexture(std::string fileName) {
        const std::string basePath = SDL_GetBasePath();
        const std::string filePath = basePath + "Content/Images/" + fileName;

        SDL_Surface* surface = IMG_Load(filePath.c_str());
        if (surface == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                         "Could not load texture %s: %s\n", filePath.c_str(),  SDL_GetError());
            return InvalidTexture;
        }

        const Uint32 textureWidth = static_cast<Uint32>(surface->w);
        const Uint32 textureHeight = static_cast<Uint32>(surface->h);

        const SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo = {
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

        const SDL_GPUTextureCreateInfo textureCreateInfo = {
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
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not acquire command buffer: %s\n", SDL_GetError());
            SDL_ReleaseGPUTexture(device, texture);
            return InvalidTexture;
        }

        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);
        if (copyPass == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not acquire copy pass: %s\n", SDL_GetError());
            return InvalidTexture;
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

        textures.push_back(texture);

        return textures.size() - 1;
    }

    void DrawSprite(const Transform &transform, const Sprite &sprite) {
        const Uint32 vertexBufferSize = sizeof(SpriteVertex) * 4; // Sprites are always quads
        const Uint32 indexBufferSize = sizeof(Uint16) * 6;
        if (spriteVertexBuffer == nullptr) {
            SDL_GPUBufferCreateInfo bufferCreateInfo = {
                .usage = SDL_GPU_BUFFERUSAGE_VERTEX |
                         SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                .size = vertexBufferSize,
                .props = 0,
            };
            spriteVertexBuffer = SDL_CreateGPUBuffer(device, &bufferCreateInfo);
            if (spriteVertexBuffer == nullptr) {
              SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                           "Could not acquire vertex buffer: %s\n", SDL_GetError());
              return;
            }
        }

        if (spriteIndexBuffer == nullptr) {
            SDL_GPUBufferCreateInfo bufferCreateInfo = {
                .usage = SDL_GPU_BUFFERUSAGE_INDEX |
                         SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                .size = indexBufferSize,
                .props = 0,
            };
            spriteIndexBuffer = SDL_CreateGPUBuffer(device, &bufferCreateInfo);
            if (spriteIndexBuffer == nullptr) {
              SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                           "Could not acquire index buffer: %s\n", SDL_GetError());
              return;
            }
        }

        if (spriteTransferBuffer == nullptr) {
            SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo = {
                .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                .size = vertexBufferSize + indexBufferSize,
            };

            spriteTransferBuffer = SDL_CreateGPUTransferBuffer(device, &transferBufferCreateInfo);
        }


        // Map vertex data
        {
            void *mappedData = SDL_MapGPUTransferBuffer(device, spriteTransferBuffer, true);
            if (mappedData == nullptr) {
                SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                        "Could not acquire sprite vertex buffer: %s\n",
                        SDL_GetError());
                return;
            }

            SpriteVertex *vertexData = (SpriteVertex*) mappedData;
            vertexData[0] = { .x = -1.0f, .y = 1.0f, .z = 0.f, .r = 1.0f, .g = 1.0f, .b = 1.0f, .u = 0.0f, .v = 0.0f };
            vertexData[1] = { .x = 1.0f, .y = 1.0f, .z = 0.f, .r = 1.0f, .g = 1.0f, .b = 1.0f, .u = 1.0f, .v = 0.0f };
            vertexData[2] = { .x = 1.0f, .y = -1.0f, .z = 0.f, .r = 1.0f, .g = 1.0f, .b = 1.0f, .u = 1.0f, .v = 1.0f };
            vertexData[3] = { .x = -1.0f, .y = -1.0f, .z = 0.f, .r = 1.0f, .g = 1.0f, .b = 1.0f, .u = 0.0f, .v = 1.0f };

            Uint16 *indexData = (Uint16*) &vertexData[4];
            indexData[0] = 0;
            indexData[1] = 1;
            indexData[2] = 2;
            indexData[3] = 0;
            indexData[4] = 2;
            indexData[5] = 3;

            SDL_UnmapGPUTransferBuffer(device, spriteTransferBuffer);
        }

        // Vertex data copy pass
        {
            SDL_GPUCommandBuffer *commandBuffer = SDL_AcquireGPUCommandBuffer(device);
            if (commandBuffer == nullptr) {
                SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                            "Could not acquire copy pass command buffer: %s\n", SDL_GetError());
                return;
            }
            SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(commandBuffer);

            SDL_GPUTransferBufferLocation vertexSrc = {
                .transfer_buffer = spriteTransferBuffer,
                .offset = 0,
            };
            SDL_GPUBufferRegion vertexDst = {
                .buffer = spriteVertexBuffer,
                .offset = 0,
                .size = vertexBufferSize
            };
            SDL_UploadToGPUBuffer(copyPass, &vertexSrc, &vertexDst, true);

            SDL_GPUTransferBufferLocation indexSrc = {
                .transfer_buffer = spriteTransferBuffer,
                .offset = vertexBufferSize,
            };
            SDL_GPUBufferRegion indexDst = {
                .buffer = spriteIndexBuffer,
                .offset = 0,
                .size = indexBufferSize,
            };
            SDL_UploadToGPUBuffer(copyPass, &indexSrc, &indexDst, true);

            SDL_EndGPUCopyPass(copyPass);
            SDL_SubmitGPUCommandBuffer(commandBuffer);
        }

        // Render pass
        {
            SDL_GPUCommandBuffer *commandBuffer = SDL_AcquireGPUCommandBuffer(device);
            if (commandBuffer == nullptr) {
              SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                           "Could not acquire render pass command buffer: %s\n", SDL_GetError());
              return;
            }

            SDL_GPUTexture *swapchainTexture;
            if (!SDL_WaitAndAcquireGPUSwapchainTexture(
                    commandBuffer, window, &swapchainTexture, nullptr, nullptr)) {
              SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                           "Could not acquire swapchain texture: %s\n", SDL_GetError());
              return;
            }

            SDL_GPUColorTargetInfo colorTargetInfo = {
                .texture = swapchainTexture,
                .mip_level = 0,
                .clear_color = CLEAR_COLOR,
                .load_op = SDL_GPU_LOADOP_CLEAR,
                .store_op = SDL_GPU_STOREOP_STORE,
                .cycle = true,
            };

            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::scale(transform, glm::vec3(sprite.scale_x, sprite.scale_y, 1.0f));
            if (swapchainTexture != nullptr) {
                SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorTargetInfo, 1, nullptr);
                if (renderPass == nullptr) {
                    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not bein render pass: %s\n",
                                SDL_GetError());
                    return;
                }

                SDL_BindGPUGraphicsPipeline(renderPass, pipelines.sprite);

                SDL_GPUBufferBinding vertexBufferBinding = {
                    .buffer = spriteVertexBuffer,
                    .offset = 0,
                };
                SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBufferBinding, 1);

                SDL_GPUBufferBinding indexBufferBinding = {
                    .buffer = spriteIndexBuffer,
                    .offset = 0,
                };
                SDL_BindGPUIndexBuffer(renderPass, &indexBufferBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

                SDL_GPUTextureSamplerBinding textureSamplerBinding = {
                    .texture = textures.at(sprite.texture_handle),
                    .sampler = samplers.nearest_clamped,
                };
                SDL_BindGPUFragmentSamplers(renderPass, 0, &textureSamplerBinding, 1);
                SDL_PushGPUVertexUniformData(commandBuffer, 0, &transform, sizeof(glm::mat4));

                SDL_DrawGPUIndexedPrimitives(renderPass, 6, 1, 0, 0, 0);

                SDL_EndGPURenderPass(renderPass);
            }
            SDL_SubmitGPUCommandBuffer(commandBuffer);
        }
    }

}
