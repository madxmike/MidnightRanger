#include "SDL3/SDL_events.h"
#include "glm/gtc/quaternion.hpp"
#include "rendering.h"
#include "transform.h"

int main() {

    rendering::InitRenderer();

    rendering::Sprite sprite = {
        .texture_handle = rendering::LoadAndRegisterTexture("test_sprite.png"),
        .scale_x = 1.0f,
        .scale_y = 1.0f,
    };

    Transform transform = {
        .x = 1.0f,
        .y = 1.0f,
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

        rendering::DrawSprite(transform, sprite);
    }

  rendering::ReleaseResources();
  return 0;
}
