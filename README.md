# LearningD3D12
Learning Game Programming with Frank D. Luna's Book "Introduction to 3D Game Programming with DirectX 12"

Try to avoid using helper libraries like d3dx12.h, which is used in the book, to better understand the underlying concepts. The code differs from that of Frank Luna.

| File | Description |
| --- | --- |
| d3dinit.h | Main header file of the program. Contains all variables used for initialization and runtime of DirectX. |
| d3dUtil.h | Contains utility functions |
| FrameResource.h | Defines wrapper class FrameResource that contains command allocator and constant buffer resources for the frame. |
| MathHelper.h | Defines several mathematical functions |
| RenderItem.h | Contains a defined mesh with its assigned CB index |
| structures.h | Contains CB and vertex structures |
| timer.h | Defines Timer class |
| UploadBuffer.h | Defines UploadBuffer class |
| window.h | Creates and operates the window using WinAPI |
| color.hlsl | Pixel shader |
| vertex.hlsl | Vertex shader |
