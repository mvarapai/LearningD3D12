/*****************************************************************//**
 * \file   d3dUtil.h
 * \brief  Contains utility functions
 * 
 * \author Mikalai Varapai
 * \date   October 2023
 *********************************************************************/
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