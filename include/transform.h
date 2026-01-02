#pragma once

#include "glm/ext/quaternion_float.hpp"

struct Transform {
    float x, y, z;
    glm::quat rotation;
};
