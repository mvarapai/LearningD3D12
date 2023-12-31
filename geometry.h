/*****************************************************************//**
 * \file   geometry.h
 * \brief  Contains geometry utility methods.
 * 
 * \author 20231063
 * \date   October 2023
 *********************************************************************/
#pragma once

#include <vector>
#include <d3d12.h>

#include "d3dUtil.h"
#include "structures.h"

 // Defines draw details of a submesh
struct SubmeshGeometry
{
    UINT IndexCount = 0;            // How many indices to draw
    UINT StartIndexLocation = 0;    // From which to start
    INT BaseVertexLocation = 0;     // Padding of the indices
};

// Class defining a mesh which could consist of multiple
// submeshes that share the same vertex and index buffers.
// Can specify user-defined vertex structure
template<typename T>
class MeshGeometry
{
private:
    // GPU resources to be used by the pipeline
    Microsoft::WRL::ComPtr<ID3D12Resource> mVertexBufferGPU = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> mIndexBufferGPU = nullptr;

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
    ID3D12GraphicsCommandList* mpCmdList = nullptr;

public:
    MeshGeometry(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCmdList)
    {
        mVertexByteStride = sizeof(T);
        mpd3dDevice = pDevice;
        mpCmdList = pCmdList;
    }

    void ConstructMeshGeometry()
    {
        // Set the remaining fields for VB and IB descriptors
        mVertexBufferByteSize = mRawVertexData.size() * mVertexByteStride;
        mIndexBufferByteSize = mRawIndexData.size() * sizeof(uint16_t);

        // Create default buffers
        mVertexBufferGPU = CreateDefaultBuffer(
            mpd3dDevice, mpCmdList, mRawVertexData.data(),
            mVertexBufferByteSize, mVertexBufferUploader);

        mIndexBufferGPU = CreateDefaultBuffer(
            mpd3dDevice, mpCmdList, mRawIndexData.data(),
            mIndexBufferByteSize, mIndexBufferUploader);
    }

    // Takes raw vertex and index data and returns associated submesh in common buffer
    SubmeshGeometry AddVertexData(std::vector<T> vertices, std::vector<uint16_t> indices)
    {
        SubmeshGeometry submesh = { };
        submesh.BaseVertexLocation = mRawVertexData.size();
        submesh.StartIndexLocation = mRawIndexData.size();
        submesh.IndexCount = indices.size();

        // Merge the vectors
        mRawVertexData.insert(std::end(mRawVertexData),
            std::begin(vertices), std::end(vertices));

        mRawIndexData.insert(std::end(mRawIndexData),
            std::begin(indices), std::end(indices));

        return submesh;
    }

    // Get binding of the vertex buffer to the pipeline
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const
    {
        D3D12_VERTEX_BUFFER_VIEW vbv = { };
        vbv.BufferLocation = mVertexBufferGPU->GetGPUVirtualAddress();
        vbv.StrideInBytes = mVertexByteStride;
        vbv.SizeInBytes = mVertexBufferByteSize;

        return vbv;
    }

    // Get binding of the index buffer to the pipeline
    D3D12_INDEX_BUFFER_VIEW IndexBufferView()const
    {
        D3D12_INDEX_BUFFER_VIEW ibv = { };
        ibv.BufferLocation = mIndexBufferGPU->GetGPUVirtualAddress();
        ibv.Format = mIndexFormat;
        ibv.SizeInBytes = mIndexBufferByteSize;

        return ibv;
    }

    // Free the memory when we have uploaded index and vertex buffers
    void DisposeUploaders()
    {
        // Since they are ComPtr, memory is released implicitly
        mVertexBufferUploader = nullptr;
        mIndexBufferUploader = nullptr;
    }
};