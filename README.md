## Ammonite Renderer
  - A repository to experiment with an OpenGL based renderer for generated data and models

## Requirements:
  - A `c++17` compatible compiler (`g++ 8+`)
  - An OpenGL 3.3+ compatible driver

## Running:
  - `make clean build` will clean the build area and build the demo from fresh
  - `./build/main` will run the built demo
  - To compile in debug mode, use `DEBUG=true make ...`
    - This will compile some extras in the code to help with debugging (every header gets `iostream`)

## Build system:
  - `make build` - Builds demo binary, a working demonstration of the renderer
  - `make clean` - Cleans the build area (`build/`)

## Dependencies:
  - `make`
  - `g++`
  - ### Libraries:
    - `libx11-dev libxi-dev libglm-dev libglfw3-dev libglew-dev libgl1-mesa-dev libglu1-mesa-dev libxrandr-dev libxext-dev libxcursor-dev libxinerama-dev libxi-dev`

## Notes:
  - All targets are compiled with `-Wall` and `-Wextra`
  - Targets are also compiled with `-O3` and `-flto`, which may cause some systems to struggle to compile, or produce unstable results
