#include "pch.h"
#include "gptInternalDXR.h"
#include "gptEntityDXR.h"
#include "gptContextDXR.h"
#include "Foundation/gptLog.h"

namespace gpt {

UINT DescriptorHandleDXR::s_stride = 0;

DescriptorHandleDXR::operator bool() const
{
    return hcpu.ptr != 0 && hgpu.ptr != 0;
}

bool DescriptorHandleDXR::operator==(const DescriptorHandleDXR& v) const
{
    return hcpu.ptr == v.hcpu.ptr && hgpu.ptr == v.hgpu.ptr;
}

bool DescriptorHandleDXR::operator!=(const DescriptorHandleDXR& v) const
{
    return !(*this == v);
}

DescriptorHandleDXR DescriptorHandleDXR::operator+(size_t n) const
{
    auto ret = *this;
    ret += n;
    return ret;
}

DescriptorHandleDXR& DescriptorHandleDXR::operator+=(size_t n)
{
    UINT offset = s_stride * UINT(n);
    hcpu.ptr += offset;
    hgpu.ptr += offset;
    return *this;
}

DescriptorHeapAllocatorDXR::DescriptorHeapAllocatorDXR()
{
}

DescriptorHeapAllocatorDXR::DescriptorHeapAllocatorDXR(ID3D12DevicePtr device, ID3D12DescriptorHeapPtr heap)
{
    reset(device, heap);
}

void DescriptorHeapAllocatorDXR::reset(ID3D12DevicePtr device, ID3D12DescriptorHeapPtr heap)
{
    if (DescriptorHandleDXR::s_stride == 0)
        DescriptorHandleDXR::s_stride = device->GetDescriptorHandleIncrementSize(heap->GetDesc().Type);
    m_hcpu = heap->GetCPUDescriptorHandleForHeapStart();
    m_hgpu = heap->GetGPUDescriptorHandleForHeapStart();
    m_capacity = heap->GetDesc().NumDescriptors;
    m_count = 0;
}

DescriptorHandleDXR DescriptorHeapAllocatorDXR::allocate(size_t n)
{
    if (m_count + n >= m_capacity) {
        // buffer depleted
        mu::DbgBreak();
    }
    DescriptorHandleDXR ret{ m_hcpu, m_hgpu };
    ret += m_count;
    m_count += (UINT)n;
    return ret;
}


FenceEventDXR::FenceEventDXR()
{
    m_handle = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

FenceEventDXR::FenceEventDXR(const FenceEventDXR &v)
{
    *this = v;
}

FenceEventDXR& FenceEventDXR::operator=(const FenceEventDXR &v)
{
    ::DuplicateHandle(::GetCurrentProcess(), v.m_handle, ::GetCurrentProcess(), &m_handle, 0, TRUE, DUPLICATE_SAME_ACCESS);
    return *this;
}

FenceEventDXR::~FenceEventDXR()
{
    ::CloseHandle(m_handle);
}
FenceEventDXR::operator HANDLE() const
{
    return m_handle;
}



CommandListManagerDXR::Record::Record(ID3D12DevicePtr device, D3D12_COMMAND_LIST_TYPE type, ID3D12PipelineStatePtr state)
{
    device->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator));
    device->CreateCommandList(0, type, allocator, state, IID_PPV_ARGS(&list));
}

void CommandListManagerDXR::Record::reset(ID3D12PipelineStatePtr state)
{
    allocator->Reset();
    list->Reset(allocator, state);
}

CommandListManagerDXR::CommandListManagerDXR(ID3D12DevicePtr device, D3D12_COMMAND_LIST_TYPE type, const wchar_t* name)
    : CommandListManagerDXR(device, type, nullptr, name)
{
}

CommandListManagerDXR::CommandListManagerDXR(ID3D12DevicePtr device, D3D12_COMMAND_LIST_TYPE type, ID3D12PipelineStatePtr state, const wchar_t* name)
    : m_device(device)
    , m_type(type)
    , m_state(state)
    , m_name(name)
{
}

ID3D12GraphicsCommandList4Ptr CommandListManagerDXR::get()
{
    ID3D12GraphicsCommandList4Ptr ret;
    if (!m_available.empty()) {
        auto& c = m_available.back();
        ret = c->list;
        m_in_use.push_back(c);
        m_available.pop_back();
    }
    else {
        auto c = std::make_shared<Record>(m_device, m_type, m_state);
        c->allocator->SetName(m_name.c_str());
        c->list->SetName(m_name.c_str());
        ret = c->list;
        m_in_use.push_back(c);
    }
    return ret;
}

void CommandListManagerDXR::reset()
{
    for (auto& p : m_in_use)
        p->reset(m_state);
    m_available.insert(m_available.end(), m_in_use.begin(), m_in_use.end());
    m_in_use.clear();
}



TimestampDXR::TimestampDXR(ID3D12DevicePtr device, int max_sample)
{
    m_max_sample = max_sample;
    if (!device)
        return;

    m_messages.resize(m_max_sample);
    {
        D3D12_QUERY_HEAP_DESC  desc{};
        desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        desc.Count = m_max_sample;
        device->CreateQueryHeap(&desc, IID_PPV_ARGS(&m_query_heap));
    }
    {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Alignment = 0;
        desc.Width = sizeof(uint64_t) * m_max_sample;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;
        device->CreateCommittedResource(&kReadbackHeapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_timestamp_buffer));
    }
}

bool TimestampDXR::valid() const
{
    return this && m_query_heap && m_timestamp_buffer;
}

bool TimestampDXR::isEnabled() const
{
    return m_enabled;
}

void TimestampDXR::setEnabled(bool v)
{
    m_enabled = v;
}

void TimestampDXR::reset()
{
    m_sample_index = 0;
}

bool TimestampDXR::query(ID3D12GraphicsCommandList4Ptr cl, const char* message)
{
    if (!valid() || !m_enabled || m_sample_index == m_max_sample)
        return false;
    int si = m_sample_index++;
    cl->EndQuery(m_query_heap, D3D12_QUERY_TYPE_TIMESTAMP, si);
    m_messages[si] = message;
    return true;
}

bool TimestampDXR::resolve(ID3D12GraphicsCommandList4Ptr cl)
{
    if (!valid() || !m_enabled)
        return false;
    cl->ResolveQueryData(m_query_heap, D3D12_QUERY_TYPE_TIMESTAMP, 0, m_sample_index, m_timestamp_buffer, 0);
    return true;
}

std::vector<std::tuple<uint64_t, std::string*>> TimestampDXR::getSamples()
{
    std::vector<std::tuple<uint64_t, std::string*>> ret;
    if (!valid() || !m_enabled)
        return ret;

    ret.resize(m_sample_index);

    uint64_t* data;
    D3D12_RANGE ragne{ 0, sizeof(uint64_t) * m_sample_index };
    auto hr = m_timestamp_buffer->Map(0, &ragne, (void**)&data);
    if (SUCCEEDED(hr)) {
        for (int si = 0; si < m_sample_index; ++si)
            ret[si] = std::make_tuple(data[si], &m_messages[si]);
        m_timestamp_buffer->Unmap(0, nullptr);
    }
    return ret;
}

void TimestampDXR::updateLog(ID3D12CommandQueuePtr cq)
{
    if (!valid() || !m_enabled || !cq)
        return;

    std::unique_lock<std::mutex> lock(m_mutex);

    m_log.clear();
    char name[256];
    char buf[256];
    auto time_samples = getSamples();
    if (!time_samples.empty()) {
        uint64_t freq = 0;
        cq->GetTimestampFrequency(&freq);

        size_t n = time_samples.size();
        auto base = time_samples[0];
        for (size_t si = 0; si < n; ++si) {
            auto s1 = time_samples[si];
            auto name1 = std::get<1>(s1);
            if (!name1)
                continue;

            auto pos1 = name1->find(" begin");
            if (pos1 != std::string::npos) {
                auto it = std::find_if(time_samples.begin() + (si + 1), time_samples.end(),
                    [&](auto& s2) {
                        auto name2 = std::get<1>(s2);
                        auto pos2 = name2->find(" end");
                        if (pos2 != std::string::npos) {
                            return pos1 == pos2 &&
                                std::strncmp(name1->c_str(), name2->c_str(), pos1) == 0;
                        }
                        return false;
                    });
                if (it != time_samples.end()) {
                    auto& s2 = *it;
                    std::strncpy(name, name1->c_str(), pos1);
                    name[pos1] = '\0';
                    auto epalsed = std::get<0>(s2) - std::get<0>(s1);
                    auto epalsed_ms = float(double(epalsed * 1000) / double(freq));
                    sprintf(buf, "%s: %.2fms\n", name, epalsed_ms);
                    m_log += buf;
                    continue;
                }
            }

            auto end_pos = name1->find(" end");
            if (pos1 == std::string::npos && end_pos == std::string::npos) {
                auto epalsed = std::get<0>(s1);
                auto epalsed_ms = float(double(epalsed * 1000) / double(freq));
                sprintf(buf, "%s: %.2fms\n", name1->c_str(), epalsed_ms);
                m_log += buf;
            }
        }
    }
}

std::string TimestampDXR::getLog()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_log;
}

void SetNameImpl(ID3D12Object* obj, LPCSTR name)
{
    if (obj && name) {
        auto wname = mu::ToWCS(name);
        obj->SetName(wname.c_str());
    }
}
void SetNameImpl(ID3D12Object* obj, LPCWSTR name)
{
    if (obj && name) {
        obj->SetName(name);
    }
}
void SetNameImpl(ID3D12Object* obj, const std::string& name)
{
    if (obj && !name.empty()) {
        auto wname = mu::ToWCS(name);
        obj->SetName(wname.c_str());
    }
}
void SetNameImpl(ID3D12Object* obj, const std::wstring& name)
{
    if (obj && !name.empty()) {
        obj->SetName(name.c_str());
    }
}


static IDXGISwapChain3Ptr CreateSwapChain(IDXGIFactory4Ptr& factory, HWND hwnd, uint32_t width, uint32_t height, DXGI_FORMAT format, ID3D12CommandQueuePtr cmd_queue)
{
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.BufferCount = gptDXRSwapChainBuffers;
    desc.Width = width;
    desc.Height = height;
    desc.Format = format;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.SampleDesc.Count = 1;

    IDXGISwapChain1Ptr swapchain;
    if (FAILED(factory->CreateSwapChainForHwnd(cmd_queue, hwnd, &desc, nullptr, nullptr, &swapchain))) {
        return nullptr;
    }

    IDXGISwapChain3Ptr ret;
    swapchain->QueryInterface(IID_PPV_ARGS(&ret));
    return ret;
}

SwapchainDXR::SwapchainDXR(ContextDXR* ctx, IWindow* window, DXGI_FORMAT format)
    : m_context(ctx)
    , m_window(base_t(window))
{
    m_swapchain = CreateSwapChain(ctx->m_dxgi_factory, (HWND)window->getHandle(), m_window->m_width, m_window->m_height, format, ctx->m_cmd_queue_direct);
    if (m_swapchain) {
        m_buffers.resize(gptDXRSwapChainBuffers);
        for (int i = 0; i < gptDXRSwapChainBuffers; ++i) {
            auto& dst = m_buffers[i];
            m_swapchain->GetBuffer(i, IID_PPV_ARGS(&dst.m_buffer));
            dst.m_rtv = ctx->m_desc_alloc.allocate();
            dst.m_uav = ctx->m_desc_alloc.allocate();
            ctx->createTextureRTV(dst.m_rtv, dst.m_buffer, format);
            ctx->createTextureUAV(dst.m_uav, dst.m_buffer);
        }
    }
}

SwapchainDXR::FrameBufferData& SwapchainDXR::getCurrentBuffer()
{
    return m_buffers[m_swapchain->GetCurrentBackBufferIndex()];
}


UINT GetTexelSize(DXGI_FORMAT rtf)
{
    switch (rtf) {
    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_UNORM:
        return 1;
    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_UNORM:
        return 2;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        return 4;

    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_FLOAT:
        return 2;
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_FLOAT:
        return 4;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        return 8;

    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_FLOAT:
        return 4;
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_FLOAT:
        return 8;
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_SINT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        return 16;
    }
    return 0;
}

DXGI_FORMAT GetDXGIFormat(Format format)
{
    switch (format) {
    case Format::Ru8: return DXGI_FORMAT_R8_TYPELESS;
    case Format::RGu8: return DXGI_FORMAT_R8G8_TYPELESS;
    case Format::RGBAu8: return DXGI_FORMAT_R8G8B8A8_TYPELESS;

    case Format::Rf16: return DXGI_FORMAT_R16_TYPELESS;
    case Format::RGf16: return DXGI_FORMAT_R16G16_TYPELESS;
    case Format::RGBAf16: return DXGI_FORMAT_R16G16B16A16_TYPELESS;

    case Format::Rf32: return DXGI_FORMAT_R32_TYPELESS;
    case Format::RGf32: return DXGI_FORMAT_R32G32_TYPELESS;
    case Format::RGBAf32: return DXGI_FORMAT_R32G32B32A32_TYPELESS;

    default: return DXGI_FORMAT_UNKNOWN;
    }
}

template<
    DXGI_FORMAT r8, DXGI_FORMAT rg8, DXGI_FORMAT rgba8,
    DXGI_FORMAT r16, DXGI_FORMAT rg16, DXGI_FORMAT rgba16,
    DXGI_FORMAT r32, DXGI_FORMAT rg32, DXGI_FORMAT rgba32>
    static inline DXGI_FORMAT _ConverterGXGIType(DXGI_FORMAT format)
{
    switch (format) {
    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_UNORM:
        return r8;

    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_UNORM:
        return rg8;

    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        return rgba8;

    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_FLOAT:
        return r16;

    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_FLOAT:
        return rg16;

    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        return rgba16;

    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_FLOAT:
        return r32;

    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_FLOAT:
        return rg32;

    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_SINT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        return rgba32;

    default: return format;
    }
}

DXGI_FORMAT GetFloatFormat(DXGI_FORMAT format)
{
    return _ConverterGXGIType<
        DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,
        DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT>(format);
}

DXGI_FORMAT GetUIntFormat(DXGI_FORMAT format)
{
    return _ConverterGXGIType<
        DXGI_FORMAT_R8_UINT, DXGI_FORMAT_R8G8_UINT, DXGI_FORMAT_R8G8B8A8_UINT,
        DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R16G16_UINT, DXGI_FORMAT_R16G16B16A16_UINT,
        DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32G32_UINT, DXGI_FORMAT_R32G32B32A32_UINT>(format);
}

DXGI_FORMAT GetTypelessFormat(DXGI_FORMAT format)
{
    return _ConverterGXGIType<
        DXGI_FORMAT_R8_TYPELESS, DXGI_FORMAT_R8G8_TYPELESS, DXGI_FORMAT_R8G8B8A8_TYPELESS,
        DXGI_FORMAT_R16_TYPELESS, DXGI_FORMAT_R16G16_TYPELESS, DXGI_FORMAT_R16G16B16A16_TYPELESS,
        DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32G32_TYPELESS, DXGI_FORMAT_R32G32B32A32_TYPELESS>(format);
}

UINT GetWidth(ID3D12Resource* v)
{
    if (!v)
        return 0;
    return (UINT)v->GetDesc().Width;
}
UINT GetHeight(ID3D12Resource* v)
{
    if (!v)
        return 0;
    return v->GetDesc().Height;
}
UINT64 GetSize(ID3D12Resource* v)
{
    if (!v)
        return 0;
    auto desc = v->GetDesc();
    UINT64 ret = desc.Width;
    if (desc.Height != 0)
        ret *= desc.Height;
    return ret;
}


std::string ToString(ID3DBlob* blob)
{
    std::string ret;
    ret.resize(blob->GetBufferSize());
    memcpy(&ret[0], blob->GetBufferPointer(), blob->GetBufferSize());
    return ret;
}

// thanks: https://github.com/microsoft/DirectX-Graphics-Samples
void PrintStateObjectDesc(const D3D12_STATE_OBJECT_DESC* desc)
{
    std::wstringstream wstr;
    wstr << L"\n";
    wstr << L"--------------------------------------------------------------------\n";
    wstr << L"| D3D12 State Object 0x" << static_cast<const void*>(desc) << L": ";
    if (desc->Type == D3D12_STATE_OBJECT_TYPE_COLLECTION) wstr << L"Collection\n";
    if (desc->Type == D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE) wstr << L"Raytracing Pipeline\n";

    auto ExportTree = [](UINT depth, UINT numExports, const D3D12_EXPORT_DESC* exports)
    {
        std::wostringstream woss;
        for (UINT i = 0; i < numExports; i++)
        {
            woss << L"|";
            if (depth > 0)
            {
                for (UINT j = 0; j < 2 * depth - 1; j++) woss << L" ";
            }
            woss << L" [" << i << L"]: ";
            if (exports[i].ExportToRename) woss << exports[i].ExportToRename << L" --> ";
            woss << exports[i].Name << L"\n";
        }
        return woss.str();
    };

    for (UINT i = 0; i < desc->NumSubobjects; i++)
    {
        wstr << L"| [" << i << L"]: ";
        switch (desc->pSubobjects[i].Type)
        {
        case D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE:
            wstr << L"Global Root Signature 0x" << desc->pSubobjects[i].pDesc << L"\n";
            break;
        case D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE:
            wstr << L"Local Root Signature 0x" << desc->pSubobjects[i].pDesc << L"\n";
            break;
        case D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK:
            wstr << L"Node Mask: 0x" << std::hex << std::setfill(L'0') << std::setw(8) << *static_cast<const UINT*>(desc->pSubobjects[i].pDesc) << std::setw(0) << std::dec << L"\n";
            break;
        case D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY:
        {
            wstr << L"DXIL Library 0x";
            auto lib = static_cast<const D3D12_DXIL_LIBRARY_DESC*>(desc->pSubobjects[i].pDesc);
            wstr << lib->DXILLibrary.pShaderBytecode << L", " << lib->DXILLibrary.BytecodeLength << L" bytes\n";
            wstr << ExportTree(1, lib->NumExports, lib->pExports);
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION:
        {
            wstr << L"Existing Library 0x";
            auto collection = static_cast<const D3D12_EXISTING_COLLECTION_DESC*>(desc->pSubobjects[i].pDesc);
            wstr << collection->pExistingCollection << L"\n";
            wstr << ExportTree(1, collection->NumExports, collection->pExports);
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION:
        {
            wstr << L"Subobject to Exports Association (Subobject [";
            auto association = static_cast<const D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION*>(desc->pSubobjects[i].pDesc);
            UINT index = static_cast<UINT>(association->pSubobjectToAssociate - desc->pSubobjects);
            wstr << index << L"])\n";
            for (UINT j = 0; j < association->NumExports; j++)
            {
                wstr << L"|  [" << j << L"]: " << association->pExports[j] << L"\n";
            }
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION:
        {
            wstr << L"DXIL Subobjects to Exports Association (";
            auto association = static_cast<const D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION*>(desc->pSubobjects[i].pDesc);
            wstr << association->SubobjectToAssociate << L")\n";
            for (UINT j = 0; j < association->NumExports; j++)
            {
                wstr << L"|  [" << j << L"]: " << association->pExports[j] << L"\n";
            }
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG:
        {
            wstr << L"Raytracing Shader Config\n";
            auto config = static_cast<const D3D12_RAYTRACING_SHADER_CONFIG*>(desc->pSubobjects[i].pDesc);
            wstr << L"|  [0]: Max Payload Size: " << config->MaxPayloadSizeInBytes << L" bytes\n";
            wstr << L"|  [1]: Max Attribute Size: " << config->MaxAttributeSizeInBytes << L" bytes\n";
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG:
        {
            wstr << L"Raytracing Pipeline Config\n";
            auto config = static_cast<const D3D12_RAYTRACING_PIPELINE_CONFIG*>(desc->pSubobjects[i].pDesc);
            wstr << L"|  [0]: Max Recursion Depth: " << config->MaxTraceRecursionDepth << L"\n";
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP:
        {
            wstr << L"Hit Group (";
            auto hitGroup = static_cast<const D3D12_HIT_GROUP_DESC*>(desc->pSubobjects[i].pDesc);
            wstr << (hitGroup->HitGroupExport ? hitGroup->HitGroupExport : L"[none]") << L")\n";
            wstr << L"|  [0]: Any Hit Import: " << (hitGroup->AnyHitShaderImport ? hitGroup->AnyHitShaderImport : L"[none]") << L"\n";
            wstr << L"|  [1]: Closest Hit Import: " << (hitGroup->ClosestHitShaderImport ? hitGroup->ClosestHitShaderImport : L"[none]") << L"\n";
            wstr << L"|  [2]: Intersection Import: " << (hitGroup->IntersectionShaderImport ? hitGroup->IntersectionShaderImport : L"[none]") << L"\n";
            break;
        }
        }
        wstr << L"|--------------------------------------------------------------------\n";
    }
    wstr << L"\n";
    OutputDebugStringW(wstr.str().c_str());
}

} // namespace gpt
