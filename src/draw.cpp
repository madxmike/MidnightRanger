#include "SDL3/SDL_gpu.h"
#include "SDL3/SDL_log.h"
#include <glm/vec3.hpp>
#include "glm/ext/matrix_float4x4.hpp"
#include <glm/ext/matrix_transform.hpp>
#include "pipelines.h"
#include "draw.h"

constexpr SDL_FColor CLEAR_COLOR{
    .r = 0.0,
    .g = 0.0,
    .b = 0.0,
    .a = 1.0,
};

SDL_GPUBuffer *SpriteVertexBuffer;
SDL_GPUBuffer *SpriteIndexBuffer;
SDL_GPUTransferBuffer *SpriteTransferBuffer;

void DrawSprite(SDL_GPUDevice *device, SDL_Window* window, SDL_GPUGraphicsPipeline *pipeline, SDL_GPUTexture *texture, SDL_GPUSampler *sampler) {
    const Uint32 vertexBufferSize = sizeof(SpriteVertex) * 4; // Sprites are always quads
    const Uint32 indexBufferSize = sizeof(Uint16) * 6;
    if (SpriteVertexBuffer == nullptr) {
        SDL_GPUBufferCreateInfo bufferCreateInfo = {
            .usage = SDL_GPU_BUFFERUSAGE_VERTEX |
                     SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
            .size = vertexBufferSize,
            .props = 0,
        };
        SpriteVertexBuffer = SDL_CreateGPUBuffer(device, &bufferCreateInfo);
        if (SpriteVertexBuffer == nullptr) {
          SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                       "Could not acquire vertex buffer: %s\n", SDL_GetError());
          return;
        }
    }

    if (SpriteIndexBuffer == nullptr) {
        SDL_GPUBufferCreateInfo bufferCreateInfo = {
            .usage = SDL_GPU_BUFFERUSAGE_INDEX |
                     SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
            .size = indexBufferSize,
            .props = 0,
        };
        SpriteIndexBuffer = SDL_CreateGPUBuffer(device, &bufferCreateInfo);
        if (SpriteIndexBuffer == nullptr) {
          SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                       "Could not acquire index buffer: %s\n", SDL_GetError());
          return;
        }
    }

    if (SpriteTransferBuffer == nullptr) {
        SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo = {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = vertexBufferSize + indexBufferSize,
        };

        SpriteTransferBuffer = SDL_CreateGPUTransferBuffer(device, &transferBufferCreateInfo);
    }


    // Map vertex data
    {
        void *mappedData = SDL_MapGPUTransferBuffer(device, SpriteTransferBuffer, true);
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

        SDL_UnmapGPUTransferBuffer(device, SpriteTransferBuffer);
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
            .transfer_buffer = SpriteTransferBuffer,
            .offset = 0,
        };
        SDL_GPUBufferRegion vertexDst = {
            .buffer = SpriteVertexBuffer,
            .offset = 0,
            .size = vertexBufferSize
        };
        SDL_UploadToGPUBuffer(copyPass, &vertexSrc, &vertexDst, true);

        SDL_GPUTransferBufferLocation indexSrc = {
            .transfer_buffer = SpriteTransferBuffer,
            .offset = vertexBufferSize,
        };
        SDL_GPUBufferRegion indexDst = {
            .buffer = SpriteIndexBuffer,
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
        transform = glm::scale(transform, glm::vec3(0.1, 0.1, 0.1));
        if (swapchainTexture != nullptr) {
            SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorTargetInfo, 1, nullptr);
            if (renderPass == nullptr) {
                SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not bein render pass: %s\n",
                            SDL_GetError());
                return;
            }

            SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

            SDL_GPUBufferBinding vertexBufferBinding = {
                .buffer = SpriteVertexBuffer,
                .offset = 0,
            };
            SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBufferBinding, 1);

            SDL_GPUBufferBinding indexBufferBinding = {
                .buffer = SpriteIndexBuffer,
                .offset = 0,
            };
            SDL_BindGPUIndexBuffer(renderPass, &indexBufferBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

            SDL_GPUTextureSamplerBinding textureSamplerBinding = {
                .texture = texture,
                .sampler = sampler,
            };
            SDL_BindGPUFragmentSamplers(renderPass, 0, &textureSamplerBinding, 1);
            SDL_PushGPUVertexUniformData(commandBuffer, 0, &transform, sizeof(glm::mat4));

            SDL_DrawGPUIndexedPrimitives(renderPass, 6, 1, 0, 0, 0);

            SDL_EndGPURenderPass(renderPass);
        }
        SDL_SubmitGPUCommandBuffer(commandBuffer);
    }
}
