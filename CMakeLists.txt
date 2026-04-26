cmake_minimum_required(VERSION 3.10)

project(global_illu)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt5Widgets REQUIRED)
find_package(Threads REQUIRED)

set(SOURCES
    main.cpp

    src/app/gui.cpp
    src/app/viewer.cpp
    src/app/scene_factory.cpp

    src/core/random.cpp
    src/scene/camera.cpp
    src/scene/scene.cpp
    src/geometry/sphere.cpp
    src/geometry/triangle.cpp
    src/geometry/plane.cpp
    src/shading/material.cpp
    src/shading/texture.cpp
    src/shading/light.cpp

    src/render/renderer.cpp
    src/render/direct_lighting_integrator.cpp
    src/render/whitted_integrator.cpp
    src/render/path_tracing_integrator.cpp

    src/accel/bbox.cpp
    src/accel/octree.cpp
    src/accel/bvh.cpp

    src/io/image.cpp
    src/io/obj_loader.cpp
)

set(HEADERS
    include/app/gui.h
    include/app/viewer.h
    include/app/scene_factory.h

    include/core/ray.h
    include/core/color.h
    include/core/random.h
    include/core/constants.h

    include/scene/hit_record.h
    include/scene/camera.h
    include/scene/scene.h
    include/geometry/primitive.h
    include/geometry/geometry.h
    include/geometry/sphere.h
    include/geometry/triangle.h
    include/geometry/plane.h
    include/shading/shading.h
    include/shading/material.h
    include/shading/texture.h
    include/shading/light.h

    include/render/renderer.h
    include/render/integrator.h
    include/render/direct_lighting_integrator.h
    include/render/whitted_integrator.h
    include/render/path_tracing_integrator.h

    include/accel/bbox.h
    include/accel/accel_structure.h
    include/accel/octree.h
    include/accel/bvh.h

    include/io/image.h
    include/io/obj_loader.h
)

add_executable(global_illu ${SOURCES} ${HEADERS})

target_include_directories(global_illu PRIVATE
    include
    3rd_party
)

target_compile_definitions(global_illu PRIVATE
    PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(global_illu
    Qt5::Widgets
    ${CMAKE_THREAD_LIBS_INIT}
)
