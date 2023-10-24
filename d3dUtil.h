#pragma once

#include <d3d12.h>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <wrl.h>

inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

class DxException
{
public:
    DxException() = default;
    DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

    std::wstring ToString()const;

    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring Filename;
    int LineNumber = -1;
};

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif

void Transition(ID3D12Resource* pResource,
    ID3D12GraphicsCommandList* commandList,
    D3D12_RESOURCE_STATES stateBefore,
    D3D12_RESOURCE_STATES stateAfter);

// Defines draw details of a submesh
struct SubmeshGeometry
{
    UINT IndexCount = 0;            // How many indices to draw
    UINT StartIndexLocation = 0;    // From which to start
    INT BaseVertexLocation = 0;     // Padding of the indices
};

// Class defining a mesh which could consist of multiple
// submeshes that share the same vertex and index buffers.
// Note: it is up to the user to properly define and upload everything,
// this class only provides the fields and getter methods.
struct MeshGeometry
{
    // Give mesh a name
    std::string Name;

    // Generic memory slots to copy raw data
    Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

    // GPU resources to be used by the pipeline
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

    // Intermediate upload heaps
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

    // Data about the buffers
    UINT VertexByteStride = 0; // Identify byte size of each vertex object
    UINT VertexBufferByteSize = 0; // Byte size of the entire VB

    DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT; // Basically IB stride
    UINT IndexBufferByteSize = 0;   // Size of the IB

    // Associate each submesh with a name.
    // Used to store multiple submeshes in one vertex and index buffer,
    // to decrease overhead in changing the buffers in use.
    std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

    // Functions to get descriptors to the buffers

    // Get binding of the vertex buffer to the pipeline
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const
    {
        D3D12_VERTEX_BUFFER_VIEW vbv = { };
        vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
        vbv.StrideInBytes = VertexByteStride;
        vbv.SizeInBytes = VertexBufferByteSize;

        return vbv;
    }

    // Get binding of the index buffer to the pipeline
    D3D12_INDEX_BUFFER_VIEW IndexBufferView()const
    {
        D3D12_INDEX_BUFFER_VIEW ibv = { };
        ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
        ibv.Format = IndexFormat;
        ibv.SizeInBytes = IndexBufferByteSize;

        return ibv;
    }

    // Free the memory when we have uploaded index and vertex buffers
    void DisposeUploaders()
    {
        // Since they are ComPtr, memory is released implicitly
        VertexBufferUploader = nullptr;
        IndexBufferUploader = nullptr;
    }
};

// Utility function to define default heap of GPU memory
const D3D12_HEAP_PROPERTIES HeapProperties(D3D12_HEAP_TYPE type);

// Utility function to define buffer description and avoid code repetition
const D3D12_RESOURCE_DESC BufferDesc(UINT64 width);

// Function that pads given int to closest multiple of 256 bits
inline UINT CalcConstantBufferByteSize(UINT byteSize)
{
    return (byteSize + 255) & ~255;
}

// Function wrapper for D3DCompileFromFile that handles errors.
// Also enables debug flags if running a debug build.
Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
    const std::wstring& filename,
    const D3D_SHADER_MACRO* defines,
    const std::string& entrypoint,
    const std::string& target);

// Utility function to create a default buffer and fill it with initData
// by creating an intermediate upload buffer.
// Note: uploadBuffer reference is provided to keep buffer alive until
// the GPU finishes uploading.
Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    const void* initData,
    UINT64 byteSize,
    _Out_ Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);

// Memory management

inline void MemcpySubresource(
    _In_ const D3D12_MEMCPY_DEST* pDest,
    _In_ const D3D12_SUBRESOURCE_DATA* pSrc,
    SIZE_T RowSizeInBytes,
    UINT NumRows,
    UINT NumSlices);

inline UINT64 UpdateSubresources(
    _In_ ID3D12GraphicsCommandList* pCmdList,
    _In_ ID3D12Resource* pDestinationResource,
    _In_ ID3D12Resource* pIntermediate,
    _In_range_(0, D3D12_REQ_SUBRESOURCES) UINT FirstSubresource,
    _In_range_(0, D3D12_REQ_SUBRESOURCES - FirstSubresource)
    UINT NumSubresources,
    UINT64 RequiredSize,
    _In_reads_(NumSubresources) const D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts,
    _In_reads_(NumSubresources) const UINT* pNumRows,
    _In_reads_(NumSubresources) const UINT64* pRowSizeInBytes,
    _In_reads_(NumSubresources) const D3D12_SUBRESOURCE_DATA* pSrcData);

template <UINT MaxSubresources>
inline UINT64 UpdateSubresources(
    _In_ ID3D12GraphicsCommandList* pCmdList,
    _In_ ID3D12Resource* pDestinationResource,
    _In_ ID3D12Resource* pIntermediate,
    UINT64 IntermediateOffset,
    _In_range_(0, MaxSubresources) UINT FirstSubresource,
    _In_range_(1, MaxSubresources - FirstSubresource) UINT NumSubresources,
    _In_reads_(NumSubresources) D3D12_SUBRESOURCE_DATA* pSrcData);