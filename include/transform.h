#pragma once

#include "glm/ext/quaternion_float.hpp"
#include "glm/ext/quaternion_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/trigonometric.hpp"

namespace transform {
    enum class Axis {
        Up,
        Right,
        Forward,
    };

    constexpr glm::vec3 VectorUp = glm::vec3(0.0f, 1.0f, 0.0);
    constexpr glm::vec3 VectorRight = glm::vec3(1.0f, 0.0f, 0.0f);
    constexpr glm::vec3 VectorForward = glm::vec3(0.0f, 0.0f, -1.0f);

    struct Transform {
        glm::vec3 position;
        glm::quat rotation;

        glm::vec3 Up() const {
            return rotation * VectorUp;
        }

        glm::vec3 Right() const {
            return rotation * VectorForward;
        }

        glm::vec3 Forward() const {
            return rotation * VectorForward;
        }

        void RotateAroundAxis(const float &degrees, const Axis &axis) {
            glm::vec3 axisVector = VectorUp;
            if (axis == Axis::Up) {
                axisVector = VectorUp;
            } else if (axis == Axis::Right) {
                axisVector = VectorRight;
            } else if (axis == Axis::Forward) {
                axisVector = VectorForward;
            }

            const float radians = glm::radians(degrees);

            rotation = glm::rotate(rotation, radians, axisVector);
        }

        void Translate(const float x, const float y) {
            this->Translate(x, y, 0.0);
        }

        void Translate(const float x, const float y, const float z) {
            this->position.x += x;
            this->position.y += y;
            this->position.z += z;
        }
    };

}
