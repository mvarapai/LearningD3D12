/*****************************************************************//**
 * \file   geometry.h
 * \brief  Contains geometry utility methods.
 * 
 * \author Mikalai Varapai
 * \date   October 2023
 *********************************************************************/
#pragma once

#include <vector>

#include "d3dUtil.h"
#include "structures.h"

// Class defining a mesh which could consist of multiple
// submeshes that share the same vertex and index buffers.
// Can specify user-defined vertex structure
template<typename T>
class StaticGeometryUploader
{
private:
    // Intermediate upload heaps
    Microsoft::WRL::ComPtr<ID3D12Resource> mVertexBufferUploader = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> mIndexBufferUploader = nullptr;

    // Data about the buffers
    UINT mVertexByteStride = 0; // Identify byte size of each vertex object
    UINT mVertexBufferByteSize = 0; // Byte size of the entire VB

    DXGI_FORMAT mIndexFormat = DXGI_FORMAT_R16_UINT; // Basically IB stride
    UINT mIndexBufferByteSize = 0;   // Size of the IB

    std::vector<T> mRawVertexData;
    std::vector<uint16_t> mRawIndexData;

    std::vector<SubmeshGeometry> mSubmeshes;

    // Pointers to D3D interfaces
    ID3D12Device* mpd3dDevice = nullptr;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mpCmdList = nullptr;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mpCmdAllocator = nullptr;

public:
    StaticGeometryUploader(ID3D12Device* pDevice)
    {
        mVertexByteStride = sizeof(T);
        mpd3dDevice = pDevice;

        // Create own command list and allocator for geometry uploader
        pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(mpCmdAllocator.GetAddressOf()));
        pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            mpCmdAllocator.Get(), nullptr, IID_PPV_ARGS(mpCmdList.GetAddressOf()));
    }

    // Constructs and uploads geometry to default GPU buffers
    void ConstructGeometry(Microsoft::WRL::ComPtr<ID3D12Resource>& pVertexBufferResource,
        Microsoft::WRL::ComPtr<ID3D12Resource>& pIndexBufferResource,
        ID3D12CommandQueue* pQueue,
        ID3D12Fence* pFence,
        UINT64& currentFence)
    {
        // Set the remaining fields for VB and IB descriptors
        mVertexBufferByteSize = static_cast<UINT>(mRawVertexData.size()) * mVertexByteStride;
        mIndexBufferByteSize = static_cast<UINT>(mRawIndexData.size()) * sizeof(uint16_t);

        // Create default buffers
        pVertexBufferResource = CreateDefaultBuffer(
            mpd3dDevice, mpCmdList.Get(), mRawVertexData.data(),
            mVertexBufferByteSize, mVertexBufferUploader);

        pIndexBufferResource = CreateDefaultBuffer(
            mpd3dDevice, mpCmdList.Get(), mRawIndexData.data(),
            mIndexBufferByteSize, mIndexBufferUploader);

        ThrowIfFailed(mpCmdList->Close());
        ID3D12CommandList* commandLists[] = { mpCmdList.Get() };
        pQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

        // Wait till the upload is finished

        currentFence++;
        pQueue->Signal(pFence, currentFence);

        if (pFence->GetCompletedValue() < currentFence)
        {
            HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false,
                EVENT_ALL_ACCESS);

            // Fire event when GPU hits current fence
            ThrowIfFailed(pFence->SetEventOnCompletion(currentFence, eventHandle));

            if (eventHandle == nullptr) return;

            WaitForSingleObject(eventHandle, INFINITE);
            CloseHandle(eventHandle);
        }

        VBBufferAddress = pVertexBufferResource->GetGPUVirtualAddress();
        IBBufferAddress = pIndexBufferResource->GetGPUVirtualAddress();

        // Now we can release uploaders
        DisposeUploaders();
    }

    const std::vector<SubmeshGeometry> GetSubmeshes()const
    {
        return mSubmeshes;
    }

private:
    // Takes raw vertex and index data and returns associated submesh in common buffer
    void AddVertexData(std::vector<T> vertices, std::vector<uint16_t> indices)
    {
        SubmeshGeometry submesh = { };
        submesh.BaseVertexLocation = static_cast<INT>(mRawVertexData.size());
        submesh.StartIndexLocation = static_cast<UINT>(mRawIndexData.size());
        submesh.IndexCount = static_cast<UINT>(indices.size());

        mSubmeshes.push_back(submesh);

        // Merge the vectors
        mRawVertexData.insert(std::end(mRawVertexData),
            std::begin(vertices), std::end(vertices));

        mRawIndexData.insert(std::end(mRawIndexData),
            std::begin(indices), std::end(indices));
    }

public:
    // Get binding of the vertex buffer to the pipeline
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const
    {
        D3D12_VERTEX_BUFFER_VIEW vbv = { };
        vbv.BufferLocation = VBBufferAddress;
        vbv.StrideInBytes = mVertexByteStride;
        vbv.SizeInBytes = mVertexBufferByteSize;

        return vbv;
    }

    // Get binding of the index buffer to the pipeline
    D3D12_INDEX_BUFFER_VIEW IndexBufferView()const
    {
        D3D12_INDEX_BUFFER_VIEW ibv = { };
        ibv.BufferLocation = IBBufferAddress;
        ibv.Format = mIndexFormat;
        ibv.SizeInBytes = mIndexBufferByteSize;

        return ibv;
    }
private:
    D3D12_GPU_VIRTUAL_ADDRESS VBBufferAddress = 0;
    D3D12_GPU_VIRTUAL_ADDRESS IBBufferAddress = 0;

    // Free the memory when we have uploaded index and vertex buffers
    void DisposeUploaders()
    {
        // Since they are ComPtr, memory is released implicitly
        mVertexBufferUploader = nullptr;
        mIndexBufferUploader = nullptr;
    }

    friend void CreateGrid(StaticGeometryUploader<Vertex>* meshGeometry, UINT numRows, float cellLength);
    friend void CreateTerrain(StaticGeometryUploader<Vertex>* meshGeometry, std::string filename);
    friend void CreatePlane(StaticGeometryUploader<Vertex>* meshGeometry, UINT n, UINT m, float width, float depth);

};

void CreateGrid(StaticGeometryUploader<Vertex>* meshGeometry, UINT numRows, float cellLength);
void CreateTerrain(StaticGeometryUploader<Vertex>* meshGeometry, std::string filename);
void CreatePlane(StaticGeometryUploader<Vertex>* meshGeometry, UINT n, UINT m, float width, float depth);


