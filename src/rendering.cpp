#include "SDL3/SDL_gpu.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_log.h"
#include "SDL3/SDL_video.h"
#include "SDL3_image/SDL_image.h"
#include "SDL3_shadercross/SDL_shadercross.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/fwd.hpp"
#include "glm/gtc/quaternion.hpp"
#include "transform.h"
#include <sys/types.h>
#include <vector>
#include "rendering.h"

namespace rendering {
    namespace {
        template <typename T>
        struct QueuedDraw {
            T element;
            transform::Transform transform;
        };

        namespace sprite {
            static const uint MaxSpriteCount = 1024;

            struct Instance {
                float width, height;
                float _padding_a, _padding_b;
                float x, y, z;
                float rotation;
                float scale_x, scale_y;
                float u, v;
                float r, g, b, a;
            };

            // Note: Data is considered valid only for the current frame and up to drawCount
            QueuedDraw<Sprite> drawQueue[MaxSpriteCount];
            Uint32 drawCount;
            SDL_GPUTransferBuffer *dataTransferBuffer;
            SDL_GPUBuffer *dataBuffer;
        };

        struct GraphicsPipelines {
            SDL_GPUGraphicsPipeline* sprite;
        };

        struct Samplers {
            SDL_GPUSampler *nearest_clamped;
        };

        static const uint WindowWidth = 800;
        static const uint WindowHeight = 600;
        static const uint Fov = 90;


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
        glm::mat4 projectionMatrix;


        // Note: Append only to keep handles consistent for the lifetime of the game
        std::vector<SDL_GPUTexture *> textures;
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
        SDL_GPUColorTargetDescription colorTargetDescription = {
            .format = textureFormat,
            .blend_state = {
                .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .color_blend_op = SDL_GPU_BLENDOP_ADD,
                .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                .enable_blend = true,
            }
        };

        SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
            .vertex_shader = LoadAndCompileShader(device, "Sprite.vert"),
            .fragment_shader = LoadAndCompileShader(device, "Sprite.frag"),
            .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
            .target_info = {
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

        projectionMatrix = glm::orthoLH<float>(0, WindowWidth, WindowHeight, 0, 0, -1.0);

        SDL_GPUTransferBufferCreateInfo spriteDataTransferBufferCreateInfo = {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = sprite::MaxSpriteCount * sizeof(sprite::Instance),
        };

        sprite::dataTransferBuffer = SDL_CreateGPUTransferBuffer(device, &spriteDataTransferBufferCreateInfo);


        SDL_GPUBufferCreateInfo spriteDataBufferCreateInfo = {
            .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
            .size = sprite::MaxSpriteCount * sizeof(sprite::Instance)
        };

        sprite::dataBuffer = SDL_CreateGPUBuffer(device, &spriteDataBufferCreateInfo);
        SDL_SetGPUBufferName(device, sprite::dataBuffer, "Sprite Data Buffer");
        SDL_GPUSwapchainComposition swapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
        SDL_SetGPUSwapchainParameters(device, window, swapchainComposition, SDL_GPU_PRESENTMODE_IMMEDIATE);
    }

    void ReleaseResources() {
        for (const auto& texture : textures) {
            SDL_ReleaseGPUTexture(device, texture);
        }

        SDL_ReleaseGPUTransferBuffer(device, sprite::dataTransferBuffer);
        SDL_ReleaseGPUBuffer(device, sprite::dataBuffer);

        SDL_ReleaseGPUGraphicsPipeline(device, pipelines.sprite);
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
        SDL_SetGPUTextureName(device, texture, fileName.c_str());

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

    void BeginFrame() {
        sprite::drawCount = 0;
    }

    void DrawSprite(const Sprite &sprite, const transform::Transform &transform) {
        sprite::drawQueue[sprite::drawCount] = {
            .element = sprite,
            .transform = transform,
        };
        sprite::drawCount += 1;
    }

    void UploadSpriteData(SDL_GPUCommandBuffer *commandBuffer) {
        sprite::Instance *data = (sprite::Instance *) SDL_MapGPUTransferBuffer(device, sprite::dataTransferBuffer, true);
        if (data == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not acquire sprite vertex buffer: %s\n", SDL_GetError());
            return;
        }

        for (int i = 0; i < sprite::drawCount; i++) {
            const QueuedDraw<Sprite> queuedDraw = sprite::drawQueue[i];
            data[i].width = 16.0f;
            data[i].height = 16.0f;
            data[i].x = queuedDraw.transform.position.x;
            data[i].y = queuedDraw.transform.position.y;
            data[i].z = queuedDraw.transform.position.z;
            data[i].rotation = 0.0; // TODO (Michael): Figure out how to determine this angle from the quaternion
            data[i].scale_x = queuedDraw.element.scale_x;
            data[i].scale_y = queuedDraw.element.scale_y;
            data[i].u = 0.0f;
            data[i].v = 0.0f;
            data[i].r = 1.0f;
            data[i].b = 1.0f;
            data[i].g = 1.0f;
            data[i].a = 1.0f;
        }

        SDL_UnmapGPUTransferBuffer(device, sprite::dataTransferBuffer);

        SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(commandBuffer);

        SDL_GPUTransferBufferLocation source = {
            .transfer_buffer = sprite::dataTransferBuffer,
            .offset = 0
        };

        SDL_GPUBufferRegion destination = {
            .buffer = sprite::dataBuffer,
            .offset = 0,
            .size = static_cast<Uint32>(sprite::drawCount * sizeof(sprite::Instance))
        };

        SDL_UploadToGPUBuffer(copyPass, &source, &destination, true);

        SDL_EndGPUCopyPass(copyPass);
    }

    void DrawSprites(SDL_GPUCommandBuffer *commandBuffer, SDL_GPUTexture *swapchainTexture, const glm::mat4 &viewMatrix) {

        SDL_GPUColorTargetInfo colorTargetInfo = {
            .texture = swapchainTexture,
            .mip_level = 0,
            .clear_color = CLEAR_COLOR,
            .load_op = SDL_GPU_LOADOP_CLEAR,
            .store_op = SDL_GPU_STOREOP_STORE,
            .cycle = false,
        };
        SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorTargetInfo, 1, nullptr);
        if (renderPass == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not begin render pass: %s\n",SDL_GetError());
            return;
        }
        glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;

        SDL_BindGPUGraphicsPipeline(renderPass, pipelines.sprite);
        SDL_BindGPUVertexStorageBuffers(renderPass, 0, &sprite::dataBuffer, 1);

        SDL_GPUTextureSamplerBinding textureSamplerBinding = {
            // TODO (Michael): I'm not sure how best to get this information, may just leave this as is until I can implement a sprite atlas
            .texture = textures.at(0),
            .sampler = samplers.nearest_clamped,
        };
        SDL_BindGPUFragmentSamplers(renderPass, 0, &textureSamplerBinding, 1);
        SDL_PushGPUVertexUniformData(commandBuffer, 0, &viewProjectionMatrix, sizeof(glm::mat4));

        SDL_DrawGPUPrimitives(renderPass, sprite::drawCount * 6, 1, 0, 0);
        SDL_EndGPURenderPass(renderPass);
    }

    void DrawFrame(const camera::Camera& camera) {
        SDL_GPUCommandBuffer *uploadCommandBuffer = SDL_AcquireGPUCommandBuffer(device);
        if (uploadCommandBuffer == nullptr) {
          SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not acquire upload command buffer: %s\n", SDL_GetError());
          return;
        }

        UploadSpriteData(uploadCommandBuffer);
        SDL_SubmitGPUCommandBuffer(uploadCommandBuffer);

        SDL_GPUCommandBuffer *renderCommandBuffer = SDL_AcquireGPUCommandBuffer(device);
        if (uploadCommandBuffer == nullptr) {
          SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not acquire render command buffer: %s\n", SDL_GetError());
          return;
        }

        SDL_GPUTexture *swapchainTexture;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(renderCommandBuffer, window, &swapchainTexture, nullptr, nullptr)) {
          SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not acquire swapchain texture: %s\n", SDL_GetError());
          return;
        }

        glm::mat4 viewMatrix = camera.View();
        DrawSprites(renderCommandBuffer, swapchainTexture, viewMatrix);

        SDL_SubmitGPUCommandBuffer(renderCommandBuffer);
    }
}
