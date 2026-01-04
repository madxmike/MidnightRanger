#include "camera.h"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/matrix.hpp"
#include "transform.h"

namespace camera {
    Camera::Camera() {
        transform = {
            .position = glm::vec3(0.0f, 0.0f, 0.0f),
            .rotation = glm::identity<glm::quat>(),
        };
    }

    void Camera::Move(const float x, const float y) {
        this->transform.Translate(x, y);
    }

    const glm::mat4 Camera::View() const {
        glm::mat4 Translation = glm::translate(glm::mat4(1.0f), -this->transform.position);
        glm::mat4 Rotation = glm::mat4_cast(this->transform.rotation);

        return Rotation * Translation;
    }

    const transform::Transform Camera::GetTransform() const {
        return this->transform;
    }
}
