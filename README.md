# Octree raymarcher
A raymarched (within the fragment shader stage) raymarcher for sparse voxel octrees.
Has basic world generation within a limited world using procedural Simplex noise (from GLM).
Has basic Phong lighting and also a CPU based raymarcher with destruction and construction.

## Building (only tested on Windows)
Run nmake in the root directory.

### Dependencies
SDL2, GLM, GLEW