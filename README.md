# Cutter
https://github.com/user-attachments/assets/927617e3-4318-427e-bd09-1a4d25782fee
### About
This is a simple demo program written in `C++11` and `Vulkan`. It features:
* Physically based shading
* Image based lighting
* Mesh deformation using async compute
* HDR bloom
* MSAA
* BC5, BC6 and BC7 multithreaded texture compression
* Shader hot reloading
* [Tracy](https://github.com/wolfpld/tracy) profiling
### Building
```cmd
git clone https://github.com/Trider12/cutter --recursive
cd cutter
cmake -Bbuild -DCMAKE_BUILD_TYPE=Release -GNinja
cmake --build build
```
**Note**: Only Windows is supported (for now).
### Running
The first time the program is run, it imports models and textures, computes environment maps and *compresses* them. This might take a few minutes depending on the CPU. This data is then stored on disk for subsequent runs.

![a](cutter.png)
