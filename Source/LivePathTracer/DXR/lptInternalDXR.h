#pragma once
#include "lptSettings.h"
#include "lptEntity.h"

namespace lpt {

#define DefPtr(_a) _COM_SMARTPTR_TYPEDEF(_a, __uuidof(_a))
DefPtr(IDXGISwapChain3);
DefPtr(IDXGIFactory4);
DefPtr(IDXGIAdapter1);
DefPtr(IDXGIResource);
DefPtr(IDXGIResource1);
DefPtr(IDxcBlobEncoding);
DefPtr(ID3D11Device);
DefPtr(ID3D11Device5);
DefPtr(ID3D11DeviceContext);
DefPtr(ID3D11DeviceContext4);
DefPtr(ID3D11Buffer);
DefPtr(ID3D11Texture2D);
DefPtr(ID3D11Query);
DefPtr(ID3D11Fence);
DefPtr(ID3D12Device);
DefPtr(ID3D12Device5);
DefPtr(ID3D12GraphicsCommandList4);
DefPtr(ID3D12CommandQueue);
DefPtr(ID3D12Fence);
DefPtr(ID3D12CommandAllocator);
DefPtr(ID3D12Resource);
DefPtr(ID3D12DescriptorHeap);
DefPtr(ID3D12StateObject);
DefPtr(ID3D12PipelineState);
DefPtr(ID3D12RootSignature);
DefPtr(ID3D12StateObjectProperties);
DefPtr(ID3D12QueryHeap);
DefPtr(ID3D12Debug);
#ifdef lptEnableD3D12GBV
    DefPtr(ID3D12Debug1);
#endif
#ifdef lptEnableD3D12DREAD
    DefPtr(ID3D12DeviceRemovedExtendedDataSettings);
    DefPtr(ID3D12DeviceRemovedExtendedData);
#endif
DefPtr(ID3DBlob);
DefPtr(IDxcCompiler);
DefPtr(IDxcLibrary);
DefPtr(IDxcBlobEncoding);
DefPtr(IDxcOperationResult);
DefPtr(IDxcBlob);
#undef DefPtr

#define align_to(_alignment, _val) (((_val + _alignment - 1) / _alignment) * _alignment)

const int lptAdaptiveCascades = 3;

struct DescriptorHandleDXR
{
    D3D12_CPU_DESCRIPTOR_HANDLE hcpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE hgpu{};

    operator bool() const;
};

class DescriptorHeapAllocatorDXR
{
public:
    DescriptorHeapAllocatorDXR();
    DescriptorHeapAllocatorDXR(ID3D12DevicePtr device, ID3D12DescriptorHeapPtr heap);

    void reset(ID3D12DevicePtr device, ID3D12DescriptorHeapPtr heap);
    DescriptorHandleDXR allocate();

private:
    D3D12_CPU_DESCRIPTOR_HANDLE m_hcpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_hgpu{};
    UINT m_stride = 0;
    UINT m_capacity = 0;
    UINT m_count = 0;
};

// thin wrapper for Windows' event
class FenceEventDXR
{
public:
    FenceEventDXR();
    FenceEventDXR(const FenceEventDXR& v);
    FenceEventDXR& operator=(const FenceEventDXR& v);
    ~FenceEventDXR();
    operator HANDLE() const;

private:
    HANDLE m_handle = nullptr;
};


class TimestampDXR
{
public:
    TimestampDXR(ID3D12DevicePtr device, int max_sample = 64);

    bool valid() const;
    bool isEnabled() const;
    void setEnabled(bool v);
    void reset();
    bool query(ID3D12GraphicsCommandList4Ptr cl, const char* message);
    bool resolve(ID3D12GraphicsCommandList4Ptr cl);

    std::vector<std::tuple<uint64_t, std::string*>> getSamples();
    void updateLog(ID3D12CommandQueuePtr cq);
    std::string getLog(); // returns copy. it is intended

private:
    ID3D12QueryHeapPtr m_query_heap;
    ID3D12ResourcePtr m_timestamp_buffer;
    bool m_enabled = true;
    int m_max_sample = 0;
    int m_sample_index = 0;
    std::vector<std::string> m_messages;
    std::string m_log;
    std::mutex m_mutex;
};
using TimestampDXRPtr = std::shared_ptr<TimestampDXR>;

#ifdef lptEnableTimestamp
#define lptTimestampInitialize(q, d)   if (!q) { q = std::make_shared<TimestampDXR>(d); }
#define lptTimestampSetEnable(q, e)    q->setEnabled(e)
#define lptTimestampReset(q)           q->reset()
#define lptTimestampQuery(q, cl, m)    q->query(cl, m)
#define lptTimestampResolve(q, cl)     q->resolve(cl)
#define lptTimestampUpdateLog(q, cq)   q->updateLog(cq)
#else lptEnableTimestamp
#define lptTimestampInitialize(...)
#define lptTimestampSetEnable(...)
#define lptTimestampReset(...)
#define lptTimestampQuery(...)
#define lptTimestampResolve(...)
#define lptTimestampUpdateLog(...)
#endif lptEnableTimestamp

void SetNameImpl(ID3D12Object* obj, LPCSTR name);
void SetNameImpl(ID3D12Object* obj, LPCWSTR name);
void SetNameImpl(ID3D12Object* obj, const std::string& name);
void SetNameImpl(ID3D12Object* obj, const std::wstring& name);
#ifdef lptEnableResourceName
#define lptSetName(res, name) SetNameImpl(res, name)
#else
#define lptSetName(...)
#endif

class CommandListManagerDXR
{
public:
    CommandListManagerDXR(ID3D12DevicePtr device, D3D12_COMMAND_LIST_TYPE type, const wchar_t* name);
    CommandListManagerDXR(ID3D12DevicePtr device, D3D12_COMMAND_LIST_TYPE type, ID3D12PipelineStatePtr state, const wchar_t* name);
    ID3D12GraphicsCommandList4Ptr get();
    void reset();

    // command lists to pass ExecuteCommandLists()
    const std::vector<ID3D12CommandList*>& getCommandLists() const;

private:
    struct Record
    {
        Record(ID3D12DevicePtr device, D3D12_COMMAND_LIST_TYPE type, ID3D12PipelineStatePtr state);
        void reset(ID3D12PipelineStatePtr state);

        ID3D12CommandAllocatorPtr allocator;
        ID3D12GraphicsCommandList4Ptr list;
    };
    using CommandPtr = std::shared_ptr<Record>;

    ID3D12DevicePtr m_device;
    D3D12_COMMAND_LIST_TYPE m_type;
    ID3D12PipelineStatePtr m_state;
    std::vector<CommandPtr> m_available, m_in_use;
    std::vector<ID3D12CommandList*> m_raw;
    std::wstring m_name;
};
using CommandListManagerDXRPtr = std::shared_ptr<CommandListManagerDXR>;



extern const D3D12_HEAP_PROPERTIES kDefaultHeapProps;
extern const D3D12_HEAP_PROPERTIES kUploadHeapProps;
extern const D3D12_HEAP_PROPERTIES kReadbackHeapProps;
static const DWORD kTimeoutMS = 3000;

UINT GetTexelSize(DXGI_FORMAT rtf);
DXGI_FORMAT GetDXGIFormat(TextureFormat format);
DXGI_FORMAT GetFloatFormat(DXGI_FORMAT format);
DXGI_FORMAT GetUIntFormat(DXGI_FORMAT format);
DXGI_FORMAT GetTypelessFormat(DXGI_FORMAT format);
UINT GetWidth(ID3D12Resource* v);
UINT GetHeight(ID3D12Resource* v);
UINT64 GetSize(ID3D12Resource* v);

std::string ToString(ID3DBlob* blob);
void PrintStateObjectDesc(const D3D12_STATE_OBJECT_DESC* desc);


template<class Body>
bool Map(ID3D12Resource* res, UINT subresource, const D3D12_RANGE* range, const Body& body)
{
    using arg_t = lambda_traits<Body>::arg<0>::type;
    arg_t dst;
    if (SUCCEEDED(res->Map(subresource, range, (void**)&dst))) {
        body(dst);
        res->Unmap(subresource, nullptr);
        return true;
    }
    else {
        return false;
    }
}
template<class Body>
bool Map(ID3D12Resource* res, const Body& body)
{
    return Map(res, 0, nullptr, body);
}

// Body : [](size_t size) -> ID3D12Resource
// return true if expanded
template<class Body>
bool ReuseOrExpandBuffer(ID3D12ResourcePtr& buf, size_t stride, size_t size, size_t minimum, const Body& body)
{
    if (buf) {
        auto capacity_in_byte = buf->GetDesc().Width;
        if (capacity_in_byte < size * stride)
            buf = nullptr;
    }
    if (!buf) {
        size_t capacity = minimum;
        while (capacity < size)
            capacity *= 2;
        body(stride * capacity);
        return true;
    }
    return false;
};

} // namespace lpt
