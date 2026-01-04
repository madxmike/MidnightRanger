#include "SDL3/SDL_events.h"
#include "SDL3/SDL_stdinc.h"
#include "SDL3/SDL_timer.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/gtc/quaternion.hpp"
#include "rendering.h"
#include "transform.h"
#include "camera.h"
#include <iostream>

int main() {

    rendering::InitRenderer();

    rendering::Sprite sprite = {
        .texture_handle = rendering::LoadAndRegisterTexture("test_sprite.png"),
        .scale_x = 32.0f,
        .scale_y = 32.0f,
    };

    SDL_srand(0);

    camera::Camera camera;

    int frameCount = 0;

    uint startTime = SDL_GetTicks();

    bool continuePlay = true;
    while (continuePlay) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                continuePlay = false;
            }
        }

        // camera.Move(-0.01f, 0.0f);

        rendering::BeginFrame();
        float distance = 25.0f;

        for (int i = 0; i < 10; i++) {
            transform::Transform transform = {
                .position = glm::vec3(
                    i * 100.f,
                    0.0f,
                    distance
                ),

                .rotation = glm::quat_cast(glm::identity<glm::mat4>()),
            };
            rendering::DrawSprite(sprite, transform);
            distance += 75.0f;
        }

        rendering::DrawFrame(camera);
        frameCount++;
    }
    uint timeElasped = SDL_GetTicks() - startTime;

    float avgFps = frameCount / (timeElasped / 1000.f);


    std::cout << "Average FPS: " << avgFps << std::endl;
    rendering::ReleaseResources();
    return 0;
}
