#include "camera.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/gtc/quaternion.hpp"
#include "transform.h"

namespace camera {
    Camera::Camera() {
        transform = {
            .position = glm::vec3(0.0f, 0.0f, 0.0f),
            .rotation = glm::quatLookAt(glm::vec3(0.0, 0.0f, -1.0f), transform::VectorUp)
        };
    }

    void Camera::Move(const float x, const float y) {
        this->transform.Translate(x, y);
    }

    const glm::mat4 Camera::View() const {
        return glm::lookAt(this->transform.position, this->transform.position + this->transform.Forward(), this->transform.Up());
    }

    const transform::Transform Camera::GetTransform() const {
        return this->transform;
    }
}
