# LearningD3D12

Project developed to learn 3D rendering pipeline in great detail using DirectX 12 and WinAPI in C++.

## Current Functionality

The application reads heightmap image from `.bmp` file, calculates geometry (vertex and index buffers), sets scene lighting and then renders the terrain with the ability for user to navigate through it with the camera.

## CPU and GPU synchronization

DirectX 12 uses command queue (ID3D12CommandQueue) to store commands before they are submitted to the GPU and executed. CPU can pass additional resources (e.g. world/view/projection matrices, textures, lighting information) to GPU through constant buffers, which can later on be accessed in shaders. The problem of synchronization arises when these resources are shared between CPU and GPU, which is what happens in most cases. For example, a constant buffer containing world matrix must be updated on per frame basis, and if GPU is still using given resource to render a frame, accessing and changing it from the CPU will present a resource hazard. Thus, when using one such constant buffer CPU and GPU must be synchronized each frame. This decreases overall performance as one of the processors will idle most of the time, and we want to keep them both busy for as much as possible.

### Solution

The solution is to use so called frame resources, which are represented by struct **FrameResource**, which contains instances of constant buffers, and its managing class **DynamicResources**. The latter class keeps track of the most recent values calculated by the CPU in stack memory by using **ConstantBufferDataCPU**, and these values are then copied into a GPU resource. The application uses 3 frame resources, meaning that CPU and GPU can only be 3 frames apart, beyond that one of the processors will have to wait. The time lag that can occur is typically negligible.

## GPU Resource Memory Allocation

When creating committed GPU resources, heap properties are specified. Corresponding structure is defined as follows:

    typedef struct D3D12_HEAP_PROPERTIES {
      D3D12_HEAP_TYPE         Type;
      D3D12_CPU_PAGE_PROPERTY CPUPageProperty;
      D3D12_MEMORY_POOL       MemoryPoolPreference;
      UINT                    CreationNodeMask;
      UINT                    VisibleNodeMask;
    } D3D12_HEAP_PROPERTIES;

Heap type specifies certain preset heap structures, and it can be either:
* `D3D12_HEAP_TYPE_DEFAULT` - typically located in GPU virtual memory space, unless using **UMA** adapter, provides efficient access from the GPU and is inaccessible from CPU.
* `D3D12_HEAP_TYPE_UPLOAD` - located in system memory and is used to efficiently upload data from CPU, which can be accessed by GPU. Used mostly for frame resources and for uploading to default resources.
* `D3D12_HEAP_TYPE_READBACK` - used to read data from GPU into CPU.

When using `D3D12_HEAP_TYPE_CUSTOM`, other members are of interest to us:

* `D3D12_CPU_PAGE_PROPERTY` - specifies rules for accessing the resource from CPU.
* `D3D12_MEMORY_POOL` - uses either `L0` pool, which is system memory, providing greater bandwidth for CPU, or `L1` which will store the resource in video memory, providing better access to it from the GPU (this option can only be selected if adapter has its own memory space, i.e. is **NUMA**).

In the application, textures, sampler states and geometry buffers are all committed to default heap, e.g. GPU memory, because they are never changed.
Constant buffers, however, are utilizing upload buffers because they are updated on per frame basis, and it is faster for CPU to write to them.

## BMP image utility

This utility was borrowed from my other project, where Raspberry Pi camera gave data in raw color format and there was a need to generate a `BMP` header for the data to be accessible by image viewing applications.

The utility supports RGB and grayscale modes and is able to freely convert between these types.
