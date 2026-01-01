cmake_minimum_required(VERSION 3.28)

include(FetchContent)

set(SDL_GIT_TAG
    "release-3.2.28"
    CACHE STRING "SDL git release tag.")

FetchContent_Declare(
    sdl-github
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG ${SDL_GIT_TAG}
)

FetchContent_MakeAvailable(sdl-github)
