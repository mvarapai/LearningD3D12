#include "d3dUtil.h"

#include <comdef.h>
#include <fstream>
#include <wrl.h>

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

// Fill in member command list
ResourceBarrier::ResourceBarrier(ID3D12GraphicsCommandList* commandList) :
    mCommandList(commandList)
{
}

void ResourceBarrier::Transition(ID3D12Resource* pResource, 
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

    mCommandList->ResourceBarrier(1, &rb);
}

const D3D12_HEAP_PROPERTIES DefaultHeap()
{
    D3D12_HEAP_PROPERTIES hp = { };
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    hp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    hp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    hp.CreationNodeMask = 1;
    hp.VisibleNodeMask = 1;

    return hp;
}
