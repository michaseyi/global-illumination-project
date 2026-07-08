# Global Illumination Methods

This is a minimal global illumination / ray tracing starter project for course exercises. The project is currently built with `C++14 + CMake + Qt5 Widgets`. When launched, it opens a GUI window and displays the rendered result of the default scene.

The default entry point is `main.cpp`, which currently:

- creates a test scene via `SceneFactory`
- uses `DirectLightingIntegrator` for direct lighting
- displays the image through `Gui` / `Viewer`

## Quick Start

The repository already includes `glm`, so the main external requirements are:

- `CMake 3.10+`
- a compiler with `C++14` support
- `Qt5 Widgets`

The standard build flow is:

```bash
cmake -S . -B build
cmake --build build -j
```

After building, run:

- macOS / Linux

```bash
./build/global_illu
```

- Windows

```powershell
.\build\Release\global_illu.exe
```

If you are using a single-config generator such as Ninja, the executable on Windows may also be located directly at `build\global_illu.exe`.

## Scenes and Models

The renderer runs headless with `--headless` and writes a PNG with `--out`.
Common flags: `--w`/`--h` (resolution), `--spp` (samples per pixel),
`--integrator path|direct|whitted`, `--max-depth`, `--threads`.

### Built-in scenes (`--scene`)

- `cornell` (default) — classic Cornell box; swap the right sphere with
  `--sphere glass|metal|diffuse`.
- `showcase` — two glass spheres and a mirror sphere.
- `starter` — the original starter scene.
- `mesh` — load a triangle mesh with `--obj <file.obj>` and shade it with
  `--obj-material diffuse|metal|glass|mirror|file`. `file` uses the materials
  and textures declared in the model's companion `.mtl` (per-face colors,
  `map_Kd` diffuse textures, emission, transparency, metalness); the other
  values override the whole mesh with a single look.

```bash
./build/global_illu --headless --scene mesh \
  --obj assets/models/teapot.obj --obj-material file \
  --w 600 --h 600 --spp 128 --out teapot.png
```

### JSON scene files

Pass a path ending in `.json` to `--scene` to build a scene from a description
file — no recompile needed. See `assets/scenes/showcase.json` for a worked
example and `src/io/scene_loader.cpp` for the full schema. It supports a camera
(with optional `aperture`/`focus` for depth of field), named `materials`
(`lambert`/`mirror`/`dielectric`/`conductor`/`emissive`, with `texture` maps),
`objects` (`sphere`/`quad`/`box`/`mesh`), and `lights` (`area`/`point`). Texture
and mesh paths resolve relative to the scene file.

```bash
./build/global_illu --headless --scene assets/scenes/showcase.json \
  --w 600 --h 600 --spp 256 --out scene.png
```

## Running on Different Systems

### macOS

Recommended tools:

- Xcode Command Line Tools
- CMake
- Qt 5

If Qt 5 is installed but `cmake` still cannot find `Qt5Widgets`, specify the Qt install prefix manually:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="<Qt5 installation directory>"
cmake --build build -j
./build/global_illu
```

In many cases, `<Qt5 installation directory>` is the Qt `clang_64` directory or the Qt prefix installed by a package manager.

### Linux

On Linux, you will need:

- `g++` or `clang++`
- `cmake`
- Qt 5 development packages

For Debian / Ubuntu, for example:

```bash
sudo apt update
sudo apt install build-essential cmake qtbase5-dev
cmake -S . -B build
cmake --build build -j
./build/global_illu
```

If you are running on a remote server, inside WSL, or in a headless environment, you will also need GUI forwarding or a desktop environment. Otherwise, the project may compile successfully but fail to open the Qt window.

### Windows

Recommended setup:

- Visual Studio 2022
- the `Desktop development with C++` workload
- Qt 5, built for a compiler compatible with your Visual Studio installation

Run the following in `x64 Native Tools Command Prompt for VS`:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Qt\5.x.x\msvc2019_64"
cmake --build build --config Release
.\build\Release\global_illu.exe
```

If your Qt installation is in a different location, replace `C:\Qt\5.x.x\msvc2019_64` with the actual path on your machine.

## Common Issues

### CMake cannot find Qt5Widgets

The current `CMakeLists.txt` uses:

```cmake
find_package(Qt5Widgets REQUIRED)
```

If Qt is installed but the package is still not found, the most common fix is to reconfigure with:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="<Qt5 installation directory>"
```

If you only have Qt 6 installed, the current repository configuration will not match it directly. In that case, you will need to adjust the `find_package(Qt5Widgets REQUIRED)` setup to use Qt 6 instead.

### The project builds, but no window appears

This is a Qt GUI application, not a command-line-only renderer. If your environment does not provide a graphical desktop or proper display forwarding, the application may not be able to show its window.

### New source files are not being compiled

If you add new `.cpp` or `.h` files, make sure to update `CMakeLists.txt` as well.

## Rendering Pipeline

The project follows roughly this flow:

1. `Scene Setup`
   - Geometry
   - Materials
   - Lights
   - Camera

2. `Image Sampling`
   - choose pixel
   - choose sample position(s) within the pixel

3. `Primary Ray Generation`
   - generate a camera ray for the current sample

4. `Ray-Scene Intersection`
   - intersect the ray with scene geometry

5. `Visible Surface Determination`
   - choose the closest valid hit
   - if no hit: return background / environment

6. `Surface Interaction Construction`
   - build hit record
   - position
   - normal
   - uv / local surface data
   - material reference

7. `Radiance Evaluation`
   - compute outgoing radiance along the current viewing direction
   - possible subcases:
     - Direct Illumination
     - Material / BSDF Evaluation
     - Light Visibility Test
     - Secondary Ray Generation
     - Recursive / Stochastic Light Transport
     - Volume / Medium Interaction

8. `Image Update and Output`
   - accumulate sample contribution
   - write to pixel
   - gamma correction / tone mapping
   - display / save

The default starter execution order can be summarized as:

1. `SceneFactory` creates a test scene
2. `Renderer` iterates over pixels
3. `Camera` generates a primary ray for the current pixel
4. `Scene::intersect()` finds the closest hit
5. the geometry fills a `HitRecord`
6. `DirectLightingIntegrator` computes radiance
7. `Image` stores the result
8. `Viewer` displays the image in the GUI

## Code Organization

### Module Overview

```text
io/         # image output and model loading
app/        # GUI, viewer, and scene presets
core/       # math-adjacent rendering basics, such as ray, constants, etc.
scene/      # camera, scene container, hit record
geometry/   # primitives and geometric hierarchies
shading/    # materials, textures, lights
render/     # renderer and integrators
accel/      # acceleration structures, such as bbox, BVH, and octree
```

### Top-Level Layout

```text
.
├── 3rd_party/      third-party libraries, currently including glm
├── assets/         example assets
├── include/        header files
├── src/            source files
├── CMakeLists.txt  build configuration
└── main.cpp        program entry point
```
