#pragma once

#include "transform.h"
#include <string>

namespace rendering {
    typedef int TextureHandle;

    constexpr TextureHandle InvalidTexture = -1;

    struct Sprite {
        TextureHandle texture_handle;
        float scale_x;
        float scale_y;
    };

    /**
     * Initializes the game window and renderer state.
     */
    void InitRenderer();

    /**
     * Releases all rendering related resources
     */
    void ReleaseResources();

    TextureHandle LoadAndRegisterTexture(std::string fileName);

    void BeginFrame();

    void DrawSprite(const Transform &transform, const Sprite &sprite);

    void DrawFrame();

}
