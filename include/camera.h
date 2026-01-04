#pragma once

#include "glm/ext/matrix_float4x4.hpp"
#include "transform.h"
namespace camera {
    class Camera {
        public:
            Camera();

            void Move(const float x, const float y);
            const transform::Transform GetTransform() const;
            const glm::mat4 View() const;
        private:
            transform::Transform transform;
    };
}
