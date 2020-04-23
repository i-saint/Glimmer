#include "pch.h"
#ifdef _WIN32
#include "Foundation/lptLog.h"
#include "lptEntityDXR.h"
#include "lptPathTracer.hlsl.h"

namespace lpt {

static const int lptMaxTraceRecursionLevel = 2;

enum class RayGenType : int
{
    Default,
    AdaptiveSampling,
    Antialiasing,
};

static const WCHAR* kRayGenShaders[]{
    L"RayGenDefault",
    L"RayGenAdaptiveSampling",
    L"RayGenAntialiasing",
};
static const WCHAR* kMissShaders[]{
    L"MissCamera",
    L"MissLight" ,
};
static const WCHAR* kHitGroups[]{
    L"CameraToObj",
    L"ObjToLights" ,
};
static const WCHAR* kAnyHitShaders[]{
    L"AnyHitCamera",
    L"AnyHitLight",
};
static const WCHAR* kClosestHitShaders[]{
    L"ClosestHitCamera",
    nullptr
};

const D3D12_HEAP_PROPERTIES kDefaultHeapProps =
{
    D3D12_HEAP_TYPE_DEFAULT,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0,
    0
};
const D3D12_HEAP_PROPERTIES kUploadHeapProps =
{
    D3D12_HEAP_TYPE_UPLOAD,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0,
    0,
};
const D3D12_HEAP_PROPERTIES kReadbackHeapProps =
{
    D3D12_HEAP_TYPE_READBACK,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0,
    0,
};


ContextDXR::ContextDXR()
{
    initializeDevice();
}

ContextDXR::~ContextDXR()
{
}

CameraDXR* ContextDXR::createCamera()
{
    auto r = new CameraDXR();
    r->m_context = this;
    return r;
}

LightDXR* ContextDXR::createLight()
{
    auto r = new LightDXR();
    r->m_context = this;
    return r;
}

RenderTargetDXR* ContextDXR::createRenderTarget()
{
    auto r = new RenderTargetDXR();
    r->m_context = this;
    return r;
}

TextureDXR* ContextDXR::createTexture()
{
    auto r = new TextureDXR();
    r->m_context = this;
    return r;
}

MaterialDXR* ContextDXR::createMaterial()
{
    auto r = new MaterialDXR();
    r->m_context = this;
    return r;
}

MeshDXR* ContextDXR::createMesh()
{
    auto r = new MeshDXR();
    r->m_context = this;
    return r;
}

MeshInstanceDXR* ContextDXR::createMeshInstance()
{
    auto r = new MeshInstanceDXR();
    r->m_context = this;
    return r;
}

SceneDXR* ContextDXR::createScene()
{
    auto r = new SceneDXR();
    r->m_context = this;
    return r;
}

void ContextDXR::renderStart(IScene* v)
{
    // todo
}

void ContextDXR::renderFinish(IScene* v)
{
    // todo
}

void* ContextDXR::getDevice()
{
    return m_device.GetInterfacePtr();
}


bool ContextDXR::initializeDevice()
{
#ifdef lptEnableD3D12DebugLayer
    {
        // enable d3d12 debug features
        ID3D12DebugPtr debug0;
        if (SUCCEEDED(::D3D12GetDebugInterface(IID_PPV_ARGS(&debug0))))
        {
            // enable debug layer
            debug0->EnableDebugLayer();

#ifdef lptEnableD3D12GBV
            ID3D12Debug1Ptr debug1;
            if (SUCCEEDED(debug0->QueryInterface(IID_PPV_ARGS(&debug1)))) {
                debug1->SetEnableGPUBasedValidation(true);
                debug1->SetEnableSynchronizedCommandQueueValidation(true);
            }
#endif // lptEnableD3D12GBV
        }
    }
#endif // lptEnableD3D12DebugLayer

#ifdef lptEnableD3D12DREAD
    {
        ID3D12DeviceRemovedExtendedDataSettingsPtr dread_settings;
        if (SUCCEEDED(::D3D12GetDebugInterface(IID_PPV_ARGS(&dread_settings)))) {
            dread_settings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dread_settings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        }
    }
#endif // lptEnableD3D12DREAD

    IDXGIFactory4Ptr dxgi_factory;
    ::CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));

    // find DXR capable adapter and create device
    auto create_device = [this, dxgi_factory](bool software) -> ID3D12Device5Ptr {
        IDXGIAdapter1Ptr adapter;
        for (uint32_t i = 0; dxgi_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (!software && (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
                continue;
            else if (software && (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0)
                continue;

            // Create the device
            ID3D12Device5Ptr device;
            HRESULT hr = ::D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));
            if (SUCCEEDED(hr)) {
                D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5{};
                hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
                if (SUCCEEDED(hr) && features5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
                    return device;
                }
            }
        }
        return nullptr;
    };

#ifdef lptForceSoftwareDevice
    m_device = create_device(true);
#else // lptForceSoftwareDevice
    m_device = create_device(false);
    if (!m_device)
        m_device = create_device(true);
#endif // lptForceSoftwareDevice

    // failed to create device (DXR is not supported)
    if (!m_device) {
        SetErrorLog("Initialization failed. DXR is not supported on this system.");
        return false;
    }

    // fence
    m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    // command queues
    auto create_command_queue = [this](ID3D12CommandQueuePtr& dst, D3D12_COMMAND_LIST_TYPE type, LPCWSTR name) {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Type = type;
        m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&dst));
        lptSetName(dst, name);
    };
    create_command_queue(m_cmd_queue_direct, D3D12_COMMAND_LIST_TYPE_DIRECT, L"Direct Queue");
    create_command_queue(m_cmd_queue_compute, D3D12_COMMAND_LIST_TYPE_COMPUTE, L"Compute Queue");
    create_command_queue(m_cmd_queue_copy, D3D12_COMMAND_LIST_TYPE_COPY, L"Copy Queue");

    m_clm_direct = std::make_shared<CommandListManagerDXR>(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT, L"Direct List");
    m_clm_copy = std::make_shared<CommandListManagerDXR>(m_device, D3D12_COMMAND_LIST_TYPE_COPY, L"Copy List");


    // root signature
    {
        D3D12_DESCRIPTOR_RANGE ranges0[] = {
            { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
        };
        D3D12_DESCRIPTOR_RANGE ranges1[] = {
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
            { D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
        };
        D3D12_DESCRIPTOR_RANGE ranges2[] = {
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 4, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
        };

        D3D12_ROOT_PARAMETER params[3]{};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[0].DescriptorTable.NumDescriptorRanges = _countof(ranges0);
        params[0].DescriptorTable.pDescriptorRanges = ranges0;

        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[1].DescriptorTable.NumDescriptorRanges = _countof(ranges1);
        params[1].DescriptorTable.pDescriptorRanges = ranges1;

        params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[2].DescriptorTable.NumDescriptorRanges = _countof(ranges2);
        params[2].DescriptorTable.pDescriptorRanges = ranges2;

        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.NumParameters = _countof(params);
        desc.pParameters = params;

        ID3DBlobPtr sig_blob, error_blob;
        HRESULT hr = ::D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig_blob, &error_blob);
        if (FAILED(hr)) {
            SetErrorLog(ToString(error_blob) + "\n");
        }
        else {
            hr = m_device->CreateRootSignature(0, sig_blob->GetBufferPointer(), sig_blob->GetBufferSize(), IID_PPV_ARGS(&m_rootsig));
            if (FAILED(hr)) {
                SetErrorLog("CreateRootSignature() failed\n");
            }
        }
        if (m_rootsig) {
            lptSetName(m_rootsig, L"Shadow Rootsig");
        }
        if (!m_rootsig)
            return false;
    }

    // setup pipeline state
    {
        std::vector<D3D12_STATE_SUBOBJECT> subobjects;
        // keep elements' address to use D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION::pSubobjectToAssociate
        subobjects.reserve(32);

        auto add_subobject = [&subobjects](D3D12_STATE_SUBOBJECT_TYPE type, auto* ptr) {
            D3D12_STATE_SUBOBJECT so;
            so.Type = type;
            so.pDesc = ptr;
            subobjects.push_back(so);
        };

        D3D12_DXIL_LIBRARY_DESC dxil_desc{};
        dxil_desc.DXILLibrary.pShaderBytecode = g_lptPathTracer;
        dxil_desc.DXILLibrary.BytecodeLength = sizeof(g_lptPathTracer);
        // zero exports means 'export all'
        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &dxil_desc);

        D3D12_HIT_GROUP_DESC hit_descs[_countof(kHitGroups)]{};
        for (int i = 0; i < _countof(hit_descs); ++i) {
            auto& hit_desc = hit_descs[i];
            hit_desc.HitGroupExport = kHitGroups[i];
            hit_desc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
            hit_desc.AnyHitShaderImport = kAnyHitShaders[i];
            hit_desc.ClosestHitShaderImport = kClosestHitShaders[i];
            add_subobject(D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hit_desc);
        }

        D3D12_RAYTRACING_SHADER_CONFIG rt_shader_desc{};
        rt_shader_desc.MaxPayloadSizeInBytes = sizeof(float) * 4;
        rt_shader_desc.MaxAttributeSizeInBytes = sizeof(float) * 2; // size of BuiltInTriangleIntersectionAttributes
        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &rt_shader_desc);

        D3D12_GLOBAL_ROOT_SIGNATURE global_rootsig{};
        global_rootsig.pGlobalRootSignature = m_rootsig;
        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &global_rootsig);

        D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_desc{};
        pipeline_desc.MaxTraceRecursionDepth = lptMaxTraceRecursionLevel;
        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pipeline_desc);

        D3D12_STATE_OBJECT_DESC pso_desc{};
        pso_desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
        pso_desc.pSubobjects = subobjects.data();
        pso_desc.NumSubobjects = (UINT)subobjects.size();

#ifdef lptDebug
        PrintStateObjectDesc(&pso_desc);
#endif // lptDebug

        auto hr = m_device->CreateStateObject(&pso_desc, IID_PPV_ARGS(&m_pipeline_state));
        if (FAILED(hr)) {
            SetErrorLog("CreateStateObject() failed\n");
            return false;
        }
        if (m_pipeline_state) {
            lptSetName(m_pipeline_state, L"Shadow Pipeline State");
        }
    }

    // setup shader table
    {
        m_shader_record_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        m_shader_record_size = align_to(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, m_shader_record_size);

        const int capacity = 32;
        auto tmp_buf = createBuffer(m_shader_record_size * capacity, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
        uint8_t* addr;
        if (SUCCEEDED(tmp_buf->Map(0, nullptr, (void**)&addr))) {
            ID3D12StateObjectPropertiesPtr sop;
            m_pipeline_state->QueryInterface(IID_PPV_ARGS(&sop));

            int shader_record_count = 0;
            auto add_shader_record = [&](const WCHAR* name) {
                if (shader_record_count++ == capacity) {
                    assert(0 && "shader_record_count exceeded its capacity");
                }
                void* sid = sop->GetShaderIdentifier(name);
                assert(sid && "shader id not found");

                auto dst = addr;
                memcpy(dst, sid, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
                dst += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

                addr += m_shader_record_size;
            };

            // ray-gen
            for (auto name : kRayGenShaders)
                add_shader_record(name);

            // miss
            for (auto name : kMissShaders)
                add_shader_record(name);

            // hit
            for (auto name : kHitGroups)
                add_shader_record(name);

            tmp_buf->Unmap(0, nullptr);

            m_shader_table = createBuffer(m_shader_record_size * capacity, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON, kDefaultHeapProps);
            lptSetName(m_shader_table, L"Shadow Shader Table");
            if (!copyBuffer(m_shader_table, tmp_buf, m_shader_record_size * capacity))
                m_shader_table = nullptr;
        }
        if (!m_shader_table)
            return false;
    }
    return true;
}

uint64_t ContextDXR::incrementFenceValue()
{
    return ++m_fence_value;
}

ID3D12ResourcePtr ContextDXR::createBuffer(uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, const D3D12_HEAP_PROPERTIES& heap_props)
{
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = flags;

    ID3D12ResourcePtr ret;
    m_device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&ret));
    return ret;
}

ID3D12ResourcePtr ContextDXR::createTexture(int width, int height, DXGI_FORMAT format)
{
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE;
    D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;

    ID3D12ResourcePtr ret;
    auto hr = m_device->CreateCommittedResource(&kDefaultHeapProps, flags, &desc, initial_state, nullptr, IID_PPV_ARGS(&ret));
    return ret;
}

void ContextDXR::addResourceBarrier(ID3D12GraphicsCommandList* cl, ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = state_before;
    barrier.Transition.StateAfter = state_after;
    cl->ResourceBarrier(1, &barrier);
}

uint64_t ContextDXR::submitResourceBarrier(ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after, uint64_t preceding_fv)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = state_before;
    barrier.Transition.StateAfter = state_after;

    auto cl = m_clm_direct->get();
    cl->ResourceBarrier(1, &barrier);
    cl->Close();
    return submitDirectCommandList(cl, preceding_fv);
}

uint64_t ContextDXR::submitDirectCommandList(ID3D12GraphicsCommandList* cl, uint64_t preceding_fv)
{
    return submitCommandList(m_cmd_queue_direct, cl, preceding_fv);
}

uint64_t ContextDXR::submitComputeCommandList(ID3D12GraphicsCommandList* cl, uint64_t preceding_fv)
{
    return submitCommandList(m_cmd_queue_compute, cl, preceding_fv);
}

uint64_t ContextDXR::submitCommandList(ID3D12CommandQueue* cq, ID3D12GraphicsCommandList* cl, uint64_t preceding_fv)
{
    if (preceding_fv != 0)
        cq->Wait(m_fence, preceding_fv);

    ID3D12CommandList* cmd_list[]{ cl };
    cq->ExecuteCommandLists(_countof(cmd_list), cmd_list);

    auto fence_value = incrementFenceValue();
    cq->Signal(m_fence, fence_value);
    return fence_value;
}


uint64_t ContextDXR::readbackBuffer(void* dst, ID3D12Resource* src, UINT64 size)
{
    auto readback_buf = createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, kReadbackHeapProps);
    lptSetName(readback_buf, L"Temporary Readback Buffer");
    auto ret = copyBuffer(readback_buf, src, size, true);
    if (ret == 0)
        return 0;

    void* mapped;
    if (SUCCEEDED(readback_buf->Map(0, nullptr, &mapped))) {
        memcpy(dst, mapped, size);
        readback_buf->Unmap(0, nullptr);
    }
    return ret;
}

uint64_t ContextDXR::uploadBuffer(ID3D12Resource* dst, const void* src, UINT64 size, bool immediate)
{
    auto upload_buf = createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    lptSetName(upload_buf, L"Temporary Upload Buffer");
    void* mapped;
    if (SUCCEEDED(upload_buf->Map(0, nullptr, &mapped))) {
        memcpy(mapped, src, size);
        upload_buf->Unmap(0, nullptr);
        if (!immediate)
            m_tmp_resources.push_back(upload_buf);
        return copyBuffer(dst, upload_buf, size, immediate);
    }
    return 0;
}

uint64_t ContextDXR::copyBuffer(ID3D12Resource* dst, ID3D12Resource* src, UINT64 size, bool immediate)
{
    if (!dst || !src || size == 0)
        return 0;

    auto cl = m_clm_copy->get();
    cl->CopyBufferRegion(dst, 0, src, 0, size);
    return submitCopy(cl, immediate);
}

uint64_t ContextDXR::readbackTexture(void* dst_, ID3D12Resource* src, UINT width, UINT height, DXGI_FORMAT format)
{
    UINT stride = SizeOfTexel(format);
    UINT width_a = align_to(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, width);
    UINT size = width_a * height * stride;
    auto readback_buf = createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, kReadbackHeapProps);
    if (!readback_buf)
        return 0;
    lptSetName(readback_buf, L"Temporary Readback Texture Buffer");

    D3D12_TEXTURE_COPY_LOCATION dst_loc{};
    dst_loc.pResource = readback_buf;
    dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst_loc.PlacedFootprint.Offset = 0;
    dst_loc.PlacedFootprint.Footprint.Format = GetTypelessFormat(format);
    dst_loc.PlacedFootprint.Footprint.Width = width;
    dst_loc.PlacedFootprint.Footprint.Height = height;
    dst_loc.PlacedFootprint.Footprint.Depth = 1;
    dst_loc.PlacedFootprint.Footprint.RowPitch = width_a * stride;

    D3D12_TEXTURE_COPY_LOCATION src_loc{};
    src_loc.pResource = src;
    src_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src_loc.SubresourceIndex = 0;

    auto cl = m_clm_copy->get();
    cl->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);
    auto ret = submitCopy(cl, true);

    char* mapped;
    D3D12_RANGE ragne{ 0, size };
    if (SUCCEEDED(readback_buf->Map(0, &ragne, (void**)&mapped))) {
        auto dst = (char*)dst_;
        for (UINT yi = 0; yi < height; ++yi) {
            memcpy(dst, mapped, width * stride);
            dst += width * stride;
            mapped += width_a * stride;
        }
        readback_buf->Unmap(0, nullptr);
    }
    return ret;
}

uint64_t ContextDXR::uploadTexture(ID3D12Resource* dst, const void* src_, UINT width, UINT height, DXGI_FORMAT format, bool immediate)
{
    UINT stride = SizeOfTexel(format);
    UINT width_a = align_to(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, width);
    UINT size = width_a * height * stride;
    auto upload_buf = createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    if (!upload_buf)
        return 0;
    lptSetName(upload_buf, L"Temporary Upload Texture Buffer");

    char* mapped;
    if (SUCCEEDED(upload_buf->Map(0, nullptr, (void**)&mapped))) {
        auto src = (const char*)src_;
        for (UINT yi = 0; yi < height; ++yi) {
            memcpy(mapped, src, width * stride);
            src += width * stride;
            mapped += width_a * stride;
        }
        upload_buf->Unmap(0, nullptr);
        if (!immediate)
            m_tmp_resources.push_back(upload_buf);

        D3D12_TEXTURE_COPY_LOCATION dst_loc{};
        dst_loc.pResource = dst;
        dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst_loc.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION src_loc{};
        src_loc.pResource = upload_buf;
        src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src_loc.PlacedFootprint.Offset = 0;
        src_loc.PlacedFootprint.Footprint.Format = GetTypelessFormat(format);
        src_loc.PlacedFootprint.Footprint.Width = width;
        src_loc.PlacedFootprint.Footprint.Height = height;
        src_loc.PlacedFootprint.Footprint.Depth = 1;
        src_loc.PlacedFootprint.Footprint.RowPitch = width_a * stride;

        auto cl = m_clm_copy->get();
        cl->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);
        return submitCopy(cl, immediate);
    }
    return 0;
}

uint64_t ContextDXR::copyTexture(ID3D12Resource* dst, ID3D12Resource* src, bool immediate, uint64_t preceding_fv)
{
    D3D12_TEXTURE_COPY_LOCATION dst_loc{};
    dst_loc.pResource = dst;
    dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst_loc.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION src_loc{};
    src_loc.pResource = src;
    src_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src_loc.SubresourceIndex = 0;

    auto cl = m_clm_copy->get();
    cl->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);
    return submitCopy(cl, immediate, preceding_fv);
}

uint64_t ContextDXR::submitCopy(ID3D12GraphicsCommandList4Ptr& cl, bool immediate, uint64_t preceding_fv)
{
    cl->Close();

    if (preceding_fv != 0)
        m_cmd_queue_copy->Wait(m_fence, preceding_fv);

    ID3D12CommandList* cmd_list[]{ cl.GetInterfacePtr() };
    m_cmd_queue_copy->ExecuteCommandLists(_countof(cmd_list), cmd_list);

    auto fence_value = incrementFenceValue();
    m_cmd_queue_copy->Signal(m_fence, fence_value);
    if (immediate) {
        m_fence->SetEventOnCompletion(fence_value, m_event_copy);
        ::WaitForSingleObject(m_event_copy, kTimeoutMS);
    }
    return fence_value;
}

} // namespace lpt

lptAPI lpt::IContext* lptCreateContextDXR()
{
    return new lpt::ContextDXR();
}

#endif // _WIN32
