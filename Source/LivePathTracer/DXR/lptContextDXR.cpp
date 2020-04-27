#include "pch.h"
#ifdef _WIN32
#include "Foundation/lptLog.h"
#include "Foundation/lptUtils.h"
#include "lptContextDXR.h"
#include "lptPathTracer.hlsl.h"

namespace lpt {

enum class RayGenType : int
{
    Default,
};

static const WCHAR* kRayGenShaders[]{
    L"RayGenRadiance",
};
static const WCHAR* kMissShaders[]{
    L"MissRadiance",
    L"MissOcclusion" ,
};
static const WCHAR* kHitGroups[]{
    L"Radiance",
    L"Occlusion" ,
};
static const WCHAR* kClosestHitShaders[]{
    L"ClosestHitRadiance",
    L"ClosestHitOcclusion",
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
    clear();
}

void ContextDXR::clear()
{
    m_cameras.clear();
    m_lights.clear();
    m_render_targets.clear();
    m_textures.clear();
    m_materials.clear();
    m_meshes.clear();
    m_mesh_instances.clear();
    m_scenes.clear();
}

ICameraPtr ContextDXR::createCamera()
{
    auto r = new CameraDXR();
    r->m_context = this;
    m_cameras.insert(r);
    return r;
}

ILightPtr ContextDXR::createLight()
{
    auto r = new LightDXR();
    r->m_context = this;
    m_lights.insert(r);
    return r;
}

IRenderTargetPtr ContextDXR::createRenderTarget(TextureFormat format, int width, int height)
{
    auto r = new RenderTargetDXR(format, width, height);
    r->m_context = this;
    m_render_targets.insert(r);
    return r;
}

ITexturePtr ContextDXR::createTexture(TextureFormat format, int width, int height)
{
    auto r = new TextureDXR(format, width, height);
    r->m_context = this;
    m_textures.insert(r);
    return r;
}

IMaterialPtr ContextDXR::createMaterial()
{
    auto r = new MaterialDXR();
    r->m_context = this;
    m_materials.insert(r);
    return r;
}

IMeshPtr ContextDXR::createMesh()
{
    auto r = new MeshDXR();
    r->m_context = this;
    m_meshes.insert(r);
    return r;
}

IMeshInstancePtr ContextDXR::createMeshInstance(IMesh* v)
{
    auto r = new MeshInstanceDXR(v);
    r->m_context = this;
    m_mesh_instances.insert(r);
    return r;
}

IScenePtr ContextDXR::createScene()
{
    auto r = new SceneDXR();
    r->m_context = this;
    m_scenes.insert(r);
    return r;
}

void ContextDXR::render()
{
    lptTimestampReset(m_timestamp);
    //lptTimestampSetEnable(m_timestamp, GetGlobals().hasDebugFlag(DebugFlag::Timestamp));

    updateEntities();
    updateResources();
    deform();
    updateBLAS();
    updateTLAS();
    dispatchRays();
}

bool ContextDXR::checkError()
{
    auto reason = m_device->GetDeviceRemovedReason();
    if (reason != 0) {
#ifdef lptEnableD3D12DREAD
        {
            ID3D12DeviceRemovedExtendedDataPtr dread;
            if (SUCCEEDED(m_device->QueryInterface(IID_PPV_ARGS(&dread)))) {
                D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumps;
                if (SUCCEEDED(dread->GetAutoBreadcrumbsOutput(&breadcrumps))) {
                    // todo: get error log
                }

                D3D12_DRED_PAGE_FAULT_OUTPUT pagefault;
                if (SUCCEEDED(dread->GetPageFaultAllocationOutput(&pagefault))) {
                    // todo: get error log
                }
            }
        }
#endif // lptEnableD3D12DREAD

        {
            PSTR buf = nullptr;
            size_t size = ::FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, reason, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, NULL);

            std::string message(buf, size);
            SetErrorLog(message.c_str());
        }
        return false;
    }
    return true;
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

    ::CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgi_factory));

    // find DXR capable adapter and create device
    auto create_device = [this](bool software) -> ID3D12Device5Ptr {
        IDXGIAdapter1Ptr adapter;
        for (uint32_t i = 0; m_dxgi_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
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

#ifdef lptEnableD3D12StablePowerState
    // try to set power stable state. this requires Windows to be developer mode.
    // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-setstablepowerstate
    if (GetGlobals().hasDebugFlag(DebugFlag::PowerStableState)) {
        if (!mu::IsDeveloperMode()) {
            SetErrorLog(
                "Enabling power stable state requires Windows to be developer mode. "
                "Check Windows Settings -> Update & Security -> For Developers -> Use developer features. "
                "(Restarting application is required to apply changes)");
        }
        else {
            auto hr = m_device->SetStablePowerState(TRUE);
            if (FAILED(hr)) {
                checkError();
            }
        }
    }
#endif


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
    m_clm_direct = std::make_shared<CommandListManagerDXR>(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT, L"Direct List");


    // root signature
    {
        // frame buffer
        D3D12_DESCRIPTOR_RANGE ranges0[] = {
            { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
        };

        // scene data
        D3D12_DESCRIPTOR_RANGE ranges1[] = {
            { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
            { D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
        };

        // global data
        D3D12_DESCRIPTOR_RANGE ranges2[] = {
            // instance / mesh / material info
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 1, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
            // vertex buffers
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, lptDXRMaxMeshCount, 0, 2, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
            // face buffers
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, lptDXRMaxMeshCount, 0, 3, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
            // textures
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, lptDXRMaxTextureCount, 0, 4, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
        };

        D3D12_ROOT_PARAMETER params[3]{};
        auto append = [&params](const int i, auto& range) {
            params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            params[i].DescriptorTable.NumDescriptorRanges = _countof(range);
            params[i].DescriptorTable.pDescriptorRanges = range;
        };
#define Append(I) static_assert(I < _countof(params), "param size exceeded"); append(I, ranges##I)
        Append(0);
        Append(1);
        Append(2);
#undef Append

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
            //hit_desc.AnyHitShaderImport = kAnyHitShaders[i];
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
        pipeline_desc.MaxTraceRecursionDepth = lptDXRMaxTraceRecursionLevel;
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

    lptTimestampInitialize(m_timestamp, m_device);

    return true;
}

void ContextDXR::updateEntities()
{
    m_scenes.eraseUnreferenced();
    m_mesh_instances.eraseUnreferenced();
    m_meshes.eraseUnreferenced();

    m_materials.eraseUnreferenced();
    m_textures.eraseUnreferenced();
    m_render_targets.eraseUnreferenced();
    m_cameras.eraseUnreferenced();
    m_lights.eraseUnreferenced();

    // clear states
    for (auto& pmesh : m_meshes) {
        pmesh->m_blas_updated = false;
    }
    for (auto& pinst : m_mesh_instances) {
        pinst->m_blas_updated = false;
    }
    m_fv_upload = m_fv_deform = m_fv_blas = m_fv_tlas = m_fv_rays = 0;


    // update entities

    each_ref(m_meshes, [&](auto& mesh) {
        if (mesh.isDirty(DirtyFlag::Shape)) {
            mesh.updateFaceNormals();
            mesh.padVertexBuffers();
        }
    });

    each_ref(m_scenes, [](auto& scene) {
        if (scene.m_camera)
            scene.m_data.camera = scene.m_camera->m_data;

        uint32_t nlights = std::min((uint32_t)scene.m_lights.size(), (uint32_t)lptMaxLights);
        scene.m_data.light_count = nlights;
        for (uint32_t li = 0; li < nlights; ++li)
            scene.m_data.lights[li] = scene.m_lights[li]->m_data;
    });
}

void ContextDXR::updateResources()
{
    m_cl = m_clm_direct->get();

    if (GetGlobals().hasDebugFlag(DebugFlag::ForceUpdateAS)) {
        // clear BLAS (for debug & measure time)
        for (auto& pobj : m_meshes)
            pobj->clearBLAS();
        for (auto& pobj : m_mesh_instances)
            pobj->clearBLAS();
    }


    // setup shader table
    if (!m_buf_shader_table) {
        m_shader_record_size = align_to(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

        createBuffer(m_buf_shader_table, m_buf_shader_table_staging, m_shader_record_size * lptDXRMaxShaderRecords, [this](char* addr) {
            ID3D12StateObjectPropertiesPtr sop;
            m_pipeline_state->QueryInterface(IID_PPV_ARGS(&sop));

            int shader_record_count = 0;
            auto add_shader_record = [&](const WCHAR* name) {
                if (shader_record_count++ == lptDXRMaxShaderRecords) {
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

            });
        lptSetName(m_buf_shader_table, L"PathTracer Shader Table");
    }

    // desc heap
    if (!m_desc_heap) {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.NumDescriptors = lptDXRMaxDescriptorCount;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_desc_heap));
        lptSetName(m_desc_heap, "Global Desc Heap");

        m_desc_alloc.reset(m_device, m_desc_heap);
        m_srv_instances = m_desc_alloc.allocate();
        m_srv_meshes = m_desc_alloc.allocate();
        m_srv_materials = m_desc_alloc.allocate();
        m_srv_vertices = m_desc_alloc.allocate(lptDXRMaxMeshCount);
        m_srv_faces = m_desc_alloc.allocate(lptDXRMaxMeshCount);
        m_srv_textures = m_desc_alloc.allocate(lptDXRMaxTextureCount);
    }

    // render targets
    each_ref(m_render_targets, [&](auto& rt) {
        rt.updateResources();
    });

    // textures
    each_ref(m_textures, [&](auto& tex) {
        tex.updateResources();
    });


    // materials
    {
        bool dirty_materials = false;
        for (auto& pmat : m_materials) {
            if (pmat->isDirty()) {
                dirty_materials = true;
                break;
            }
        }

        if (dirty_materials) {
            bool allocated = createBuffer(m_buf_materials, m_buf_materials_staging, sizeof(MaterialData) * m_materials.capacity(), [this](MaterialData* dst) {
                for (auto& pmat : m_materials)
                    dst[pmat->m_id] = pmat->m_data;
                });
            if (allocated) {
                lptSetName(m_buf_materials, "Material Buffer");
                createBufferSRV(m_srv_materials, m_buf_materials, sizeof(MaterialData));
            }
        }
    }

    // mesh
    {
        bool dirty_meshes = false;
        each_ref(m_meshes, [&](auto& mesh) {
            mesh.updateResources();
            if (mesh.isDirty())
                dirty_meshes = true;
        });

        if (dirty_meshes) {
            bool allocated = createBuffer(m_buf_meshes, m_buf_meshes_staging, sizeof(MeshData) * m_meshes.capacity(), [this](MeshData* dst) {
                for (auto& pmesh : m_meshes)
                    dst[pmesh->m_id] = pmesh->m_data;
            });
            if (allocated) {
                lptSetName(m_buf_meshes, "Mesh Buffer");
                createBufferSRV(m_srv_meshes, m_buf_meshes, sizeof(MeshData));
            }
        }
    }

    // instance
    {
        bool dirty_instances = false;
        each_ref(m_mesh_instances, [&](auto& inst) {
            inst.updateResources();
            if (inst.isDirty())
                dirty_instances = true;
        });

        if (dirty_instances) {
            bool allocated = createBuffer(m_buf_instances, m_buf_instances_staging, sizeof(InstanceData) * m_mesh_instances.capacity(), [this](InstanceData* dst) {
                for (auto& pinst : m_mesh_instances)
                    dst[pinst->m_id] = pinst->m_data;
            });
            if (allocated) {
                lptSetName(m_buf_instances, "Instance Buffer");
                createBufferSRV(m_srv_instances, m_buf_instances, sizeof(InstanceData));
            }
        }
    }

    // scene
    each_ref(m_scenes, [&](auto& scene) {
        scene.updateResources();
    });

    m_fv_upload = submit();
}

void ContextDXR::deform()
{
    //// todo
    //bool gpu_skinning = rd.hasFlag(RenderFlag::GPUSkinning) && m_deformer;
    //if (gpu_skinning) {
    //    int deform_count = 0;
    //    m_deformer->prepare(rd);
    //    for (auto& inst_dxr : rd.instances) {
    //        if (m_deformer->deform(rd, *inst_dxr))
    //            ++deform_count;
    //    }
    //    m_deformer->flush(rd);
    //}
    //else {
        m_fv_deform = m_fv_upload;
    //}
}

void ContextDXR::updateBLAS()
{
    m_cl = m_clm_direct->get();

    // build BLAS
    lptTimestampQuery(m_timestamp, m_cl, "Building BLAS begin");
    each_ref(m_meshes, [&](auto& mesh) {
        mesh.updateBLAS();
    });
    each_ref(m_mesh_instances, [&](auto& inst) {
        inst.updateBLAS();
    });
    lptTimestampQuery(m_timestamp, m_cl, "Building BLAS end");

    m_fv_blas = submit(m_fv_deform);
}

void ContextDXR::updateTLAS()
{
    m_cl = m_clm_direct->get();

    lptTimestampQuery(m_timestamp, m_cl, "Building TLAS begin");
    each_ref(m_scenes, [&](auto& scene) {
        scene.updateTLAS();
    });
    lptTimestampQuery(m_timestamp, m_cl, "Building TLAS end");

    m_fv_tlas = submit(m_fv_blas);
}

void ContextDXR::dispatchRays()
{
    m_cl = m_clm_direct->get();
    auto& cl_rays = m_cl;
    lptTimestampQuery(m_timestamp, cl_rays, "DispatchRays begin");

    cl_rays->SetComputeRootSignature(m_rootsig);
    cl_rays->SetPipelineState1(m_pipeline_state.GetInterfacePtr());

    ID3D12DescriptorHeap* desc_heaps[] = { m_desc_heap };
    cl_rays->SetDescriptorHeaps(_countof(desc_heaps), desc_heaps);

    // global descriptors
    cl_rays->SetComputeRootDescriptorTable(2, m_srv_instances.hgpu);

    auto do_dispatch = [&](ID3D12Resource* rt, RayGenType raygen_type) {
        addResourceBarrier(rt, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        auto rt_desc = rt->GetDesc();

        D3D12_DISPATCH_RAYS_DESC dr_desc{};
        dr_desc.Width = (UINT)rt_desc.Width;
        dr_desc.Height = (UINT)rt_desc.Height;
        dr_desc.Depth = 1;

        auto addr = m_buf_shader_table->GetGPUVirtualAddress();
        // ray-gen
        dr_desc.RayGenerationShaderRecord.StartAddress = addr + (m_shader_record_size * (int)raygen_type);
        dr_desc.RayGenerationShaderRecord.SizeInBytes = m_shader_record_size;
        addr += m_shader_record_size * _countof(kRayGenShaders);

        // miss
        dr_desc.MissShaderTable.StartAddress = addr;
        dr_desc.MissShaderTable.StrideInBytes = m_shader_record_size;
        dr_desc.MissShaderTable.SizeInBytes = m_shader_record_size * _countof(kMissShaders);
        addr += dr_desc.MissShaderTable.SizeInBytes;

        // hit
        dr_desc.HitGroupTable.StartAddress = addr;
        dr_desc.HitGroupTable.StrideInBytes = m_shader_record_size;
        dr_desc.HitGroupTable.SizeInBytes = m_shader_record_size * _countof(kHitGroups);
        addr += dr_desc.HitGroupTable.SizeInBytes;

        cl_rays->DispatchRays(&dr_desc);
        addResourceBarrier(rt, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
    };

    // dispatch for each enabled scene
    each_ref(m_scenes, [&](auto& scene) {
        if (!scene.m_enabled || !scene.m_render_target)
            return;

        auto& rt = dxr_t(*scene.m_render_target);
        cl_rays->SetComputeRootDescriptorTable(0, scene.m_uav_frame_buffer.hgpu);
        cl_rays->SetComputeRootDescriptorTable(1, scene.m_uav_accum_buffer.hgpu);
        do_dispatch(rt.m_frame_buffer, RayGenType::Default);
    });
    lptTimestampQuery(m_timestamp, cl_rays, "DispatchRays end");


    // handle render target readback
    lptTimestampQuery(m_timestamp, cl_rays, "Readback begin");
    each_ref(m_render_targets, [&](auto& rt) {
        if (rt.m_readback_enabled)
            copyTexture(rt.m_buf_readback, rt.m_frame_buffer, rt.m_width, rt.m_height, GetDXGIFormat(rt.m_format));
    });
    lptTimestampQuery(m_timestamp, cl_rays, "Readback end");

    lptTimestampResolve(m_timestamp, cl_rays);

    // submit
    m_fv_rays = submit(m_fv_tlas);
}

void ContextDXR::finish()
{
    // wait for complete
    if (m_fv_rays != 0) {
        m_fence->SetEventOnCompletion(m_fv_rays, m_fence_event);
        ::WaitForSingleObject(m_fence_event, kTimeoutMS);
        lptTimestampUpdateLog(m_timestamp, m_cmd_queue_direct);
    }

    // reset state
    m_clm_direct->reset();

    // clear dirty flags
    m_scenes.clearDirty();
    m_mesh_instances.clearDirty();
    m_meshes.clearDirty();
    m_materials.clearDirty();
    m_textures.clearDirty();
    m_render_targets.clearDirty();
    m_cameras.clearDirty();
    m_lights.clearDirty();
}

void* ContextDXR::getDevice()
{
    return m_device.GetInterfacePtr();
}

const char* ContextDXR::getTimestampLog()
{
#ifdef lptEnableTimestamp
    static std::string s_log;
    if (m_timestamp)
        s_log = m_timestamp->getLog();
    return s_log.c_str();
#else
    return "";
#endif
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

ID3D12ResourcePtr ContextDXR::createBuffer(uint64_t size)
{
    return createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON, kDefaultHeapProps);
}

ID3D12ResourcePtr ContextDXR::createUploadBuffer(uint64_t size)
{
    return createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
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

ID3D12ResourcePtr ContextDXR::createTextureUploadBuffer(int width, int height, DXGI_FORMAT format)
{
    UINT stride = GetTexelSize(format);
    UINT width_a = align_to(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, width);
    UINT size = width_a * height * stride;
    return createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
}

ID3D12ResourcePtr ContextDXR::createTextureReadbackBuffer(int width, int height, DXGI_FORMAT format)
{
    UINT texel_size = GetTexelSize(format);
    UINT width_a = align_to(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, width);
    UINT size = width_a * height * texel_size;
    return createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, kReadbackHeapProps);
}


uint64_t ContextDXR::submit(uint64_t preceding_fv)
{
    // insert wait if preceding_fv is valid
    if (preceding_fv != 0) {
        m_cmd_queue_direct->Wait(m_fence, preceding_fv);
    }

    // close command list and submit
    m_cl->Close();
    checkError();
    ID3D12CommandList* cmd_list[]{ m_cl };
    m_cmd_queue_direct->ExecuteCommandLists(_countof(cmd_list), cmd_list);
    m_cl = nullptr;


    // insert signal
    auto fence_value = incrementFenceValue();
    m_cmd_queue_direct->Signal(m_fence, fence_value);
    return fence_value;
}

void ContextDXR::addResourceBarrier(ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = state_before;
    barrier.Transition.StateAfter = state_after;
    m_cl->ResourceBarrier(1, &barrier);
}


void ContextDXR::uploadBuffer(ID3D12Resource* dst, ID3D12Resource* staging, const void* src, UINT64 size)
{
    writeBuffer(dst, staging, size, [&](void* mapped) {
        memcpy(mapped, src, size);
    });
}

void ContextDXR::copyBuffer(ID3D12Resource* dst, ID3D12Resource* src, UINT64 size)
{
    if (!dst || !src || size == 0) {
        return;
    }

    m_cl->CopyBufferRegion(dst, 0, src, 0, size);
}

void ContextDXR::readbackBuffer(void* dst, ID3D12Resource* staging, UINT64 size)
{
    if (!dst || !staging || size == 0) {
        return;
    }

    Map(staging, [dst, size](void* mapped) {
        memcpy(dst, mapped, size);
    });
}

bool ContextDXR::createBuffer(ID3D12ResourcePtr& dst, ID3D12ResourcePtr& staging, const void* src, size_t size)
{
    return createBuffer(dst, staging, size, [&](void* mapped) {
        memcpy(mapped, src, size);
        });
}


void ContextDXR::uploadTexture(ID3D12Resource* dst, ID3D12Resource* staging, const void* src_, UINT width, UINT height, DXGI_FORMAT format)
{
    writeTexture(dst, staging, width, height, format, [&](char* mapped) {
        auto* src = (const char*)src_;
        UINT texel_size = GetTexelSize(format);
        UINT width_a = align_to(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, width);
        for (UINT yi = 0; yi < height; ++yi) {
            memcpy(mapped, src, width * texel_size);
            src += width * texel_size;
            mapped += width_a * texel_size;
        }
    });
}

void ContextDXR::uploadTexture(ID3D12Resource* dst, ID3D12Resource* staging, UINT width, UINT height, DXGI_FORMAT format)
{
    UINT texel_size = GetTexelSize(format);
    UINT width_a = align_to(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, width);

    D3D12_TEXTURE_COPY_LOCATION dst_loc{};
    dst_loc.pResource = dst;
    dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst_loc.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION src_loc{};
    src_loc.pResource = staging;
    src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src_loc.PlacedFootprint.Offset = 0;
    src_loc.PlacedFootprint.Footprint.Format = GetTypelessFormat(format);
    src_loc.PlacedFootprint.Footprint.Width = width;
    src_loc.PlacedFootprint.Footprint.Height = height;
    src_loc.PlacedFootprint.Footprint.Depth = 1;
    src_loc.PlacedFootprint.Footprint.RowPitch = width_a * texel_size;

    m_cl->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);
}

void ContextDXR::copyTexture(ID3D12Resource* dst, ID3D12Resource* src, UINT width, UINT height, DXGI_FORMAT format)
{
    if (!dst || !src) {
        return;
    }

    UINT texel_size = GetTexelSize(format);
    UINT width_a = align_to(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, width);
    UINT size = width_a * height * texel_size;

    D3D12_TEXTURE_COPY_LOCATION dst_loc{};
    dst_loc.pResource = dst;
    dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst_loc.PlacedFootprint.Offset = 0;
    dst_loc.PlacedFootprint.Footprint.Format = GetTypelessFormat(format);
    dst_loc.PlacedFootprint.Footprint.Width = width;
    dst_loc.PlacedFootprint.Footprint.Height = height;
    dst_loc.PlacedFootprint.Footprint.Depth = 1;
    dst_loc.PlacedFootprint.Footprint.RowPitch = width_a * texel_size;

    D3D12_TEXTURE_COPY_LOCATION src_loc{};
    src_loc.pResource = src;
    src_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src_loc.SubresourceIndex = 0;

    m_cl->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);
}

void ContextDXR::readbackTexture(void* dst_, ID3D12Resource* staging, UINT width, UINT height, DXGI_FORMAT format)
{
    if (!dst_ || !staging) {
        return;
    }

    UINT texel_size = GetTexelSize(format);
    UINT width_a = align_to(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, width);
    UINT size = width_a * height * texel_size;

    auto dst = (char*)dst_;
    D3D12_RANGE ragne{ 0, size };
    Map(staging, 0, &ragne, [&](char* mapped) {
        for (UINT yi = 0; yi < height; ++yi) {
            memcpy(dst, mapped, width * texel_size);
            dst += width * texel_size;
            mapped += width_a * texel_size;
        }
        });
}


void ContextDXR::createBufferSRV(DescriptorHandleDXR& handle, ID3D12Resource* res, size_t stride)
{
    if (!res)
        return;
    uint64_t capacity = GetSize(res);
    D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Buffer.FirstElement = 0;
    desc.Buffer.NumElements = UINT(capacity / stride);
    desc.Buffer.StructureByteStride = UINT(stride);
    desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    m_device->CreateShaderResourceView(res, &desc, handle.hcpu);
}

void ContextDXR::createTextureSRV(DescriptorHandleDXR& handle, ID3D12Resource* res)
{
    if (!res)
        return;
    auto rd = res->GetDesc();
    D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
    desc.Format = GetFloatFormat(rd.Format);
    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Texture2D.MipLevels = rd.MipLevels;
    m_device->CreateShaderResourceView(res, &desc, handle.hcpu);
}

void ContextDXR::createTextureUAV(DescriptorHandleDXR& handle, ID3D12Resource* res)
{
    if (!res)
        return;
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uav_desc.Format = GetFloatFormat(res->GetDesc().Format);
    m_device->CreateUnorderedAccessView(res, nullptr, &uav_desc, handle.hcpu);
}

void ContextDXR::createCBV(DescriptorHandleDXR& handle, ID3D12Resource* res, size_t size)
{
    if (!res)
        return;
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc{};
    cbv_desc.BufferLocation = res->GetGPUVirtualAddress();
    cbv_desc.SizeInBytes = UINT(size);
    m_device->CreateConstantBufferView(&cbv_desc, handle.hcpu);
}

} // namespace lpt

lptAPI lpt::IContext* lptCreateContextDXR_()
{
    return new lpt::ContextDXR();
}

#endif // _WIN32
