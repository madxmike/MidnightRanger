#include "SDL3/SDL_events.h"
#include "SDL3/SDL_stdinc.h"
#include "SDL3/SDL_timer.h"
#include "glm/gtc/quaternion.hpp"
#include "rendering.h"
#include "transform.h"
#include <iostream>

int main() {

    rendering::InitRenderer();

    rendering::Sprite sprite = {
        .texture_handle = rendering::LoadAndRegisterTexture("test_sprite.png"),
        .scale_x = 0.5f,
        .scale_y = 0.5f,
    };


    SDL_srand(0);


    int frameCount = 0;

    uint startTime = SDL_GetTicks();
    Transform transform = {
        .x = 0.0f,
        .y = 0.0f,
        .z = 0.0f,
        .rotation = glm::quat_cast(glm::mat4(1.0f)),
    };
    bool continuePlay = true;
    while (continuePlay) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                continuePlay = false;
            }
        }

        rendering::BeginFrame();

        for (int i = 0; i < 10000; i++) {

            rendering::DrawSprite(transform, sprite);
        }

        rendering::DrawFrame();
        frameCount++;
    }
    uint timeElasped = SDL_GetTicks() - startTime;

    float avgFps = frameCount / (timeElasped / 1000.f);


    std::cout << "Average FPS: " << avgFps << std::endl;
    rendering::ReleaseResources();
    return 0;
}
