# Project Themes

This practical course is centered around extending the provided ray tracing starter project.

The starter already gives you a runnable C++ framework with a basic rendering pipeline. Your task is to develop your own ray tracer further across all four project themes.

All four themes should be represented in your project to some degree. However, you are free to decide in which one or more themes you want to go deeper and make more substantial extensions.

The four themes below are meant as guidance, not as strict boundaries. You may combine ideas from multiple themes as long as you can clearly explain what you implemented and where it fits into the rendering pipeline.

## General Expectations

- Start from the provided starter framework.
- Make progress in all four themes.
- Decide yourself in which one or more themes you want to go deeper.
- Make sure your implementation has a visible or measurable effect.
- Be able to explain:
  - where your work enters the rendering pipeline,
  - what data it needs,
  - what output it produces,
  - what assumptions it makes,
  - and what its limitations are.

## Theme 1: Geometry, Scene Representation and Acceleration

### Focus

How should a scene be represented, organized, and queried so that rays can find visible surfaces correctly and efficiently?

### Typical topics

- analytic ray-geometry intersections
- triangle meshes
- hit records and surface normals
- bounding boxes
- BVH or octree acceleration
- scene hierarchies or grouping

### Example extensions

- add a new geometric primitive
- load and render triangle meshes from file
- integrate a BVH or octree into scene traversal
- compare brute-force intersection with accelerated intersection

### Typical code areas

- `scene/*`
- `geometry/*`
- `accel/*`
- optionally `io/*`

### Good fit if you enjoy

- geometry
- algorithms and data structures
- performance and scalability
- building the structural backbone of a renderer

## Theme 2: Appearance, Materials and Light Interaction

### Focus

Once a ray hits a surface, how do materials, textures, light sources, and visibility determine the visible appearance?

### Typical topics

- Lambertian reflection
- local illumination models
- BRDF basics
- texture mapping
- UV coordinates
- point lights, directional lights, or area lights
- hard and soft shadows

### Example extensions

- implement a new material or shading model beyond Lambert
- add texture mapping to one or more surface types
- implement a new light source model
- add hard or soft shadow variants

### Typical code areas

- `shading/*`
- `render/direct_lighting_integrator.*`
- optionally `geometry/*` if additional surface data is needed

### Good fit if you enjoy

- visual appearance
- shading and material behavior
- textures and lighting
- comparing rendered looks across different models

## Theme 3: Light Transport and Global Illumination

### Focus

How can the renderer go beyond direct illumination and account for reflection, refraction, and indirect light transport?

### Typical topics

- recursive ray tracing
- mirror reflection
- refraction
- Fresnel effects
- Whitted-style rendering
- path tracing
- Monte Carlo integration
- indirect illumination
- Russian roulette

### Example extensions

- implement mirror or glass materials in a Whitted-style integrator
- add recursive reflection or refraction
- implement a minimal path tracer
- compare direct lighting, Whitted rendering, and path tracing

### Typical code areas

- `render/*`
- `shading/material.*`
- optionally `core/random.*`

### Good fit if you enjoy

- the core light transport problem
- recursion and stochastic algorithms
- physically based rendering ideas
- understanding why global illumination is harder than local shading

## Theme 4: Camera Optics, Sampling and Participating Media

### Focus

How do sampling strategies, camera models, and volumetric effects influence the final image beyond the simplest pinhole-camera renderer?

### Typical topics

- anti-aliasing
- multi-sampling
- stratified or jittered sampling
- thin lens camera
- depth of field
- motion blur
- fog and transmittance
- participating media
- ray marching

### Example extensions

- add anti-aliasing through improved pixel sampling
- implement depth of field with a thin lens camera
- add motion blur through time sampling
- implement homogeneous fog or another simple medium effect

### Typical code areas

- `scene/camera.*`
- `render/*`
- `core/random.*`
- potentially new medium-related files

### Good fit if you enjoy

- image quality and sampling
- camera models
- photography-inspired rendering effects
- volumetric phenomena such as fog

## Final Note

These themes are intentionally broad. The goal is not to tick off a checklist of features, but to make a meaningful extension to the starter renderer and understand how your work fits into the overall rendering pipeline.

You are encouraged to experiment, compare results, and document your design choices clearly.
