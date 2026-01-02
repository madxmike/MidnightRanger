cmake_minimum_required(VERSION 3.28)

include(FetchContent)

FetchContent_Declare(
    glm-github
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG "1.0.3"
)

FetchContent_MakeAvailable(glm-github)
