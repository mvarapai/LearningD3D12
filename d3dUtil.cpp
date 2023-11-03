#include "d3dUtil.h"

#include <comdef.h>
#include <fstream>
#include <wrl.h>
#include <d3dcompiler.h>

using Microsoft::WRL::ComPtr;

DxException::DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber) :
    ErrorCode(hr),
    FunctionName(functionName),
    Filename(filename),
    LineNumber(lineNumber) { }

std::wstring DxException::ToString()const
{
    // Get the string description of the error code.
    _com_error err(ErrorCode);
    std::wstring msg = err.ErrorMessage();

    return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
}

void Transition(ID3D12Resource* pResource,
    ID3D12GraphicsCommandList* commandList,
    D3D12_RESOURCE_STATES stateBefore,
    D3D12_RESOURCE_STATES stateAfter)
{
    D3D12_RESOURCE_TRANSITION_BARRIER transition = { };
    transition.pResource = pResource;
    transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    transition.StateBefore = stateBefore;
    transition.StateAfter = stateAfter;

    D3D12_RESOURCE_BARRIER rb = { };
    rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    rb.Transition = transition;

    commandList->ResourceBarrier(1, &rb);
}

const D3D12_HEAP_PROPERTIES HeapProperties(D3D12_HEAP_TYPE type)
{
    D3D12_HEAP_PROPERTIES hp = { };
    hp.Type = type;
    hp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    hp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    hp.CreationNodeMask = 1;
    hp.VisibleNodeMask = 1;

    return hp;
}

const D3D12_RESOURCE_DESC BufferDesc(UINT64 width)
{
    D3D12_RESOURCE_DESC bufferDesc = { };

    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Alignment = 0;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.SampleDesc.Quality = 0;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // The buffer byte size
    bufferDesc.Width = width;

    return bufferDesc;
}

Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
    const std::wstring& filename,
    const D3D_SHADER_MACRO* defines,
    const std::string& entrypoint,
    const std::string& target)
{
    UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = S_OK;

    ComPtr<ID3DBlob> byteCode = nullptr;
    ComPtr<ID3DBlob> errors = nullptr;

    hr = D3DCompileFromFile(filename.c_str(),
        defines,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entrypoint.c_str(),
        target.c_str(),
        compileFlags,
        0,
        byteCode.GetAddressOf(),
        errors.GetAddressOf());

    // Handle errors
    if (errors != nullptr)
    {
        OutputDebugStringA((char*)errors->GetBufferPointer());
    }
    ThrowIfFailed(hr);
    return byteCode;
}

// Utility function to that creates default buffer and uploads
// the data specified through the upload buffer.
//
// Note: the upload buffer is an output parameter and has to be kept alive.
Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    const void* initData,
    UINT64 byteSize,
    _Out_ Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
{
    // Resource for output
    ComPtr<ID3D12Resource> defaultBuffer = nullptr;

    // Non-custom heaps only require the heap type (default, upload, readback)
    const D3D12_HEAP_PROPERTIES defaultHeap = 
        HeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    const D3D12_HEAP_PROPERTIES uploadHeap =
        HeapProperties(D3D12_HEAP_TYPE_UPLOAD);

    // Buffer resource description only requires byte size
    const D3D12_RESOURCE_DESC bufferDesc = BufferDesc(byteSize);

    // Proceed to create GPU resources

    ThrowIfFailed(device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(defaultBuffer.GetAddressOf())));
    ThrowIfFailed(device->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

    // Create subresource data
    D3D12_SUBRESOURCE_DATA subResourceData = { };
    subResourceData.pData = initData;

    // Neither of these shall be used since it's a buffer
    subResourceData.RowPitch = byteSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    // Create transition
    Transition(defaultBuffer.Get(),
        cmdList,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST);

    UpdateSubresources<1>(cmdList, defaultBuffer.Get(),
        uploadBuffer.Get(), 0, 0, 1, &subResourceData);

    Transition(defaultBuffer.Get(),
        cmdList,
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_GENERIC_READ);

    // Keep upload buffer alive
    return defaultBuffer;
}

inline void MemcpySubresource(
    _In_ const D3D12_MEMCPY_DEST* pDest,
    _In_ const D3D12_SUBRESOURCE_DATA* pSrc,
    SIZE_T RowSizeInBytes,
    UINT NumRows,
    UINT NumSlices)
{
    // Iterate through slices (z-coordinate, support for multiple textures)
    for (UINT z = 0; z < NumSlices; z++)
    {
        // Pointers to the start of texture 
        BYTE* pDestSlice = reinterpret_cast<BYTE*>(pDest->pData)
            + pDest->SlicePitch * z;
        const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>(pSrc->pData)
            + pSrc->SlicePitch * z;

        // Iterate through rows and copy
        for (UINT y = 0; y < NumRows; y++)
        {
            memcpy(
                pDestSlice + y * pDest->RowPitch, // Pointers to the start
                pSrcSlice + y * pSrc->RowPitch,   // of yth row
                RowSizeInBytes);
        }
    }
}

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
    _In_reads_(NumSubresources) const D3D12_SUBRESOURCE_DATA* pSrcData)
{   
    D3D12_RESOURCE_DESC IntermediateDesc = pIntermediate->GetDesc();
    D3D12_RESOURCE_DESC DestinationDesc = pDestinationResource->GetDesc();

    // Validation
    if (IntermediateDesc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER ||
        IntermediateDesc.Width < RequiredSize + pLayouts[0].Offset ||
        RequiredSize >(SIZE_T) - 1 ||
        (DestinationDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER &&
            (FirstSubresource != 0 || NumSubresources != 1)))
    {
        return 0;
    }

    // Step 1: load data to the upload buffer

    BYTE* pData = nullptr;
    // Map pData to the upload buffer
    HRESULT hr = pIntermediate->Map(0, nullptr,
        reinterpret_cast<void**>(&pData));
    if (FAILED(hr))
    {
        return 0;
    }

    // Iterate through all subresources
    for (UINT i = 0; i < NumSubresources; i++)
    {
        if (pRowSizeInBytes[i] > (SIZE_T)-1) return 0;

        D3D12_MEMCPY_DEST DestData = {
            pData + pLayouts[i].Offset, // Pointer to the start of subresource
            pLayouts[i].Footprint.RowPitch, // Row pitch
            // Slice pitch is row pitch times the number of rows
            pLayouts[i].Footprint.RowPitch * pNumRows[i] };

        // Copy each sybresource to (pData + offset)
        MemcpySubresource(&DestData, &pSrcData[i],
            (SIZE_T)pRowSizeInBytes[i], pNumRows[i],
            pLayouts[i].Footprint.Depth);
    }

    pIntermediate->Unmap(0, nullptr);

    // Step 2: copy from upload buffer to the resource

    // If we are working with buffer resource
    if (DestinationDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
        pCmdList->CopyBufferRegion(
            pDestinationResource,
            0,
            pIntermediate,
            pLayouts[0].Offset,
            pLayouts[0].Footprint.Width);
    }
    else // If we are working with texture resource
    {
        // Iterate through subresources
        for (UINT i = 0; i < NumSubresources; i++)
        {
            D3D12_TEXTURE_COPY_LOCATION Dst = { };
            Dst.pResource = pDestinationResource;
            Dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            Dst.SubresourceIndex = i + FirstSubresource;

            D3D12_TEXTURE_COPY_LOCATION Src = { };
            Src.pResource = pIntermediate;
            Src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            Src.PlacedFootprint = pLayouts[i];

            pCmdList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
        }
    }
    return RequiredSize;
}

template <UINT MaxSubresources>
inline UINT64 UpdateSubresources(
    _In_ ID3D12GraphicsCommandList* pCmdList,
    _In_ ID3D12Resource* pDestinationResource,
    _In_ ID3D12Resource* pIntermediate,
    UINT64 IntermediateOffset,
    _In_range_(0, MaxSubresources) UINT FirstSubresource,
    _In_range_(1, MaxSubresources - FirstSubresource) UINT NumSubresources,
    _In_reads_(NumSubresources) D3D12_SUBRESOURCE_DATA* pSrcData)
{
    UINT64 RequiredSize = 0;

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT Layouts[MaxSubresources];
    UINT NumRows[MaxSubresources];
    UINT64 RowSizesInBytes[MaxSubresources];

    D3D12_RESOURCE_DESC Desc = pDestinationResource->GetDesc();

    ID3D12Device* pDevice = nullptr;

    pDestinationResource->GetDevice(__uuidof(*pDevice),
        reinterpret_cast<void**>(&pDevice));

    pDevice->GetCopyableFootprints(&Desc, FirstSubresource,
        NumSubresources, IntermediateOffset, Layouts,
        NumRows, RowSizesInBytes, &RequiredSize);

    pDevice->Release();

    return UpdateSubresources(pCmdList, pDestinationResource,
        pIntermediate, FirstSubresource,
        NumSubresources, RequiredSize, Layouts,
        NumRows, RowSizesInBytes, pSrcData);
}