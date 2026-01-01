cmake_minimum_required(VERSION 3.28)

include(FetchContent)

set(SDL_IMAGE_GIT_TAG
    "release-3.2.6"
    CACHE STRING "SDL_Image git release tag.")

FetchContent_Declare(
    sdl_image-github
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG ${SDL_IMAGE_GIT_TAG}
)

FetchContent_MakeAvailable(sdl_image-github)
