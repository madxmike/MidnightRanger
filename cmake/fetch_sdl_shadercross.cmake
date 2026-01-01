cmake_minimum_required(VERSION 3.28)

include(FetchContent)

FetchContent_Declare(
    sdl_shadercross-github
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_shadercross.git
    GIT_TAG 7b7365a86611b2a7b6462e521cf1c43a037d0970
)

FetchContent_MakeAvailable(sdl_shadercross-github)
