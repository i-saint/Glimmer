#include "pch.h"
#ifdef _WIN32
#include "Foundation/lptLog.h"
#include "Foundation/lptUtils.h"
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
    m_cameras.push_back(r);
    return r;
}

LightDXR* ContextDXR::createLight()
{
    auto r = new LightDXR();
    r->m_context = this;
    m_lights.push_back(r);
    return r;
}

RenderTargetDXR* ContextDXR::createRenderTarget()
{
    auto r = new RenderTargetDXR();
    r->m_context = this;
    m_render_targets.push_back(r);
    return r;
}

TextureDXR* ContextDXR::createTexture()
{
    auto r = new TextureDXR();
    r->m_context = this;
    m_textures.push_back(r);
    return r;
}

MaterialDXR* ContextDXR::createMaterial()
{
    auto r = new MaterialDXR();
    r->m_context = this;
    m_materials.push_back(r);
    return r;
}

MeshDXR* ContextDXR::createMesh()
{
    auto r = new MeshDXR();
    r->m_context = this;
    m_meshes.push_back(r);
    return r;
}

MeshInstanceDXR* ContextDXR::createMeshInstance()
{
    auto r = new MeshInstanceDXR();
    r->m_context = this;
    m_mesh_instances.push_back(r);
    return r;
}

SceneDXR* ContextDXR::createScene()
{
    auto r = new SceneDXR();
    r->m_context = this;
    m_scenes.push_back(r);
    return r;
}

void ContextDXR::frameBegin()
{
    auto erase_unreferenced = [](auto& container) {
        erase_if(container, [](auto& obj) { return obj->getRef() == 0; });
    };
    erase_unreferenced(m_cameras);
    erase_unreferenced(m_lights);
    erase_unreferenced(m_render_targets);
    erase_unreferenced(m_textures);
    erase_unreferenced(m_materials);
    erase_unreferenced(m_meshes);
    erase_unreferenced(m_mesh_instances);
    erase_unreferenced(m_scenes);

    for (auto& obj : m_cameras) {
        if (obj->isDirty()) {
            // todo
            obj->clearDirty();
        }
    }

    for (auto& obj : m_lights) {
        if (obj->isDirty()) {
            // todo
            obj->clearDirty();
        }
    }

    for (auto& obj : m_textures) {
        if (obj->isDirty()) {
            // todo
            obj->clearDirty();
        }
    }

    for (auto& obj : m_materials) {
        if (obj->isDirty()) {
            // todo
            obj->clearDirty();
        }
    }

    for (auto& obj : m_meshes) {
        if (obj->isDirty()) {
            // todo
            obj->clearDirty();
        }
    }

    for (auto& obj : m_mesh_instances) {
        if (obj->isDirty()) {
            // todo
            obj->clearDirty();
        }
    }

    for (auto& obj : m_scenes) {
        if (obj->isDirty()) {
            // todo
            obj->clearDirty();
        }
    }
}

void ContextDXR::setupMeshes()
{
    // clear state
    for (auto& pmesh : m_meshes) {
        pmesh->m_blas_updated = false;
    }
    for (auto& pinst : m_mesh_instances) {
        pinst->m_blas_updated = false;
    }
    m_fv_upload = m_fv_deform = m_fv_blas = 0;



    // create vertex buffers
    auto create_buffer = [this](const void* buffer, size_t size) {
        auto res = createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON, kDefaultHeapProps);
        uploadBuffer(res, buffer, size);
        return res;
    };
    for (auto& pmesh : m_meshes) {
        auto& mesh = *pmesh;
        if (mesh.m_points.empty() || mesh.m_indices.empty())
            continue;

        // indices
        if (!mesh.m_buf_indices) {
            mesh.m_buf_indices = create_buffer(mesh.m_indices.data(), mesh.m_indices.size() * sizeof(int));
            lptSetName(mesh.m_buf_indices, mesh.m_name + " IB");

#ifdef lptEnableBufferValidation
            if (mesh.m_buf_indices) {
                RawVector<int> index_buffer_data32;
                index_buffer_data32.resize(mesh.m_indices.size(), 0xffffffff);
                readbackBuffer(index_buffer_data32.data(), mesh.m_buf_indices, mesh.m_indices.size() * sizeof(int));
            }
#endif // lptEnableBufferValidation
        }

        // points
        if (!mesh.m_buf_points) {
            mesh.m_buf_points = create_buffer(mesh.m_points.data(), mesh.m_points.size() * sizeof(float3));
            lptSetName(mesh.m_buf_points, mesh.m_name + " VB");

#ifdef lptEnableBufferValidation
            if (mesh.m_buf_points) {
                // inspect buffer
                RawVector<float3> vertex_buffer_data;
                vertex_buffer_data.resize(mesh.m_points.size(), mu::nan<float3>());
                readbackBuffer(vertex_buffer_data.data(), mesh.m_buf_points, mesh.m_points.size() * sizeof(float3));
            }
#endif // lptEnableBufferValidation
        }
    }

    //// deform
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
    //    rd.fv_deform = rd.fv_translate;
    //}


    // build BLAS
    auto cl_blas = m_clm_direct->get();
    lptTimestampQuery(m_timestamp, cl_blas, "Building BLAS begin");
    for (auto& pmesh : m_meshes) {
        auto& mesh = *pmesh;
        if (!mesh.m_blas || mesh.isDirty(DirtyFlag::Points)) {
            // BLAS for non-deformable meshes

            bool perform_update = mesh.m_blas != nullptr;

            D3D12_RAYTRACING_GEOMETRY_DESC geom_desc{};
            geom_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            geom_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

            geom_desc.Triangles.VertexBuffer.StartAddress = mesh.m_buf_points->GetGPUVirtualAddress();
            geom_desc.Triangles.VertexBuffer.StrideInBytes = sizeof(float3);
            geom_desc.Triangles.VertexCount = (UINT)mesh.m_points.size();
            geom_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

            geom_desc.Triangles.IndexBuffer = mesh.m_buf_indices->GetGPUVirtualAddress();
            geom_desc.Triangles.IndexCount = (UINT)mesh.m_indices.size();
            geom_desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
            inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
            if (mesh.m_buf_points && mesh.m_dynamic) {
                inputs.Flags =
                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE |
                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
                if (perform_update)
                    inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
            }
            else
                inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
            inputs.NumDescs = 1;
            inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            inputs.pGeometryDescs = &geom_desc;

            if (!mesh.m_blas) {
                D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
                m_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

                mesh.m_blas_scratch = createBuffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
                mesh.m_blas = createBuffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);
                lptSetName(mesh.m_blas_scratch, mesh.m_name + " BLAS Scratch");
                lptSetName(mesh.m_blas, mesh.m_name + " BLAS");
            }

            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC as_desc{};
            as_desc.Inputs = inputs;
            if (perform_update)
                as_desc.SourceAccelerationStructureData = mesh.m_blas->GetGPUVirtualAddress();
            as_desc.DestAccelerationStructureData = mesh.m_blas->GetGPUVirtualAddress();
            as_desc.ScratchAccelerationStructureData = mesh.m_blas_scratch->GetGPUVirtualAddress();

            cl_blas->BuildRaytracingAccelerationStructure(&as_desc, 0, nullptr);
            mesh.m_blas_updated = true;
        }
        mesh.clearDirty();
    }

    for (auto& pinst : m_mesh_instances) {
        auto& inst = *pinst;
        auto& mesh = *inst.m_mesh;

        if (inst.m_buf_points_deformed) {
            if (!inst.m_blas_deformed || inst.isDirty(DirtyFlag::Deform)) {
                // BLAS for deformable meshes

                bool perform_update = inst.m_blas_deformed != nullptr;

                D3D12_RAYTRACING_GEOMETRY_DESC geom_desc{};
                geom_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
                geom_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

                geom_desc.Triangles.VertexBuffer.StartAddress = inst.m_buf_points_deformed->GetGPUVirtualAddress();
                geom_desc.Triangles.VertexBuffer.StrideInBytes = sizeof(float3);
                geom_desc.Triangles.VertexCount = (UINT)mesh.m_points.size();
                geom_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

                geom_desc.Triangles.IndexBuffer = mesh.m_buf_indices->GetGPUVirtualAddress();
                geom_desc.Triangles.IndexCount = (UINT)mesh.m_indices.size();
                geom_desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

                D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
                inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
                inputs.Flags =
                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE |
                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
                if (perform_update)
                    inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
                inputs.NumDescs = 1;
                inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
                inputs.pGeometryDescs = &geom_desc;

                if (!inst.m_blas_deformed) {
                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
                    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

                    inst.m_blas_scratch = createBuffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
                    inst.m_blas_deformed = createBuffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);
                    lptSetName(inst.m_blas_scratch, inst.m_name + " BLAS Scratch");
                    lptSetName(inst.m_blas_deformed, inst.m_name + " BLAS");
                }

                D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC as_desc{};
                as_desc.Inputs = inputs;
                if (perform_update)
                    as_desc.SourceAccelerationStructureData = inst.m_blas_deformed->GetGPUVirtualAddress();
                as_desc.DestAccelerationStructureData = inst.m_blas_deformed->GetGPUVirtualAddress();
                as_desc.ScratchAccelerationStructureData = inst.m_blas_scratch->GetGPUVirtualAddress();

                addResourceBarrier(cl_blas, inst.m_buf_points_deformed, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                cl_blas->BuildRaytracingAccelerationStructure(&as_desc, 0, nullptr);
                addResourceBarrier(cl_blas, inst.m_buf_points_deformed, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                inst.m_blas_updated = true;
            }
        }

        if (!inst.m_blas_updated) {
            if (inst.isDirty(DirtyFlag::Deform) || mesh.m_blas_updated) {
                // transform or mesh are updated. TLAS needs to be updated.
                inst.m_blas_updated = true;
            }
        }

        inst.clearDirty();
    }

    lptTimestampQuery(m_timestamp, cl_blas, "Building BLAS end");
    cl_blas->Close();
    m_fv_blas = submitDirectCommandList(cl_blas, m_fv_deform);
}

void ContextDXR::setupScene(SceneDXR& scene)
{
    size_t instance_count = scene.m_instances.size();
    bool needs_build_tlas = true;

    if (!needs_build_tlas) {
        // if there are no BLAS updates, check geometry list is the same as last render.
        // if true, no TLAS update is needed.
        needs_build_tlas = scene.m_instances != scene.m_instances_prev;
    }

    // build TLAS
    auto cl_tlas = m_clm_direct->get();
    lptTimestampQuery(scene.m_timestamp, cl_tlas, "Building TLAS begin");
    if (needs_build_tlas) {
        auto& td = scene.m_tlas_data;

        // get the size of the TLAS buffers
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

        // instance desc buffer
        ReuseOrExpandBuffer(td.instance_desc, sizeof(D3D12_RAYTRACING_INSTANCE_DESC), instance_count, 4096, [this, &scene](size_t size) {
            auto ret = createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
            lptSetName(ret, scene.m_name + " Instance Desk");
            return ret;
            });

        // create instance desc
        {
            UINT num_descs = 0;

            D3D12_RAYTRACING_INSTANCE_DESC* instance_descs;
            td.instance_desc->Map(0, nullptr, (void**)&instance_descs);
            for (size_t i = 0; i < instance_count; i++) {
                auto& inst = *scene.m_instances[i];
                auto& mesh = *inst.m_mesh;

                bool deformed = inst.m_buf_points_deformed;
                auto& blas = deformed ? inst.m_blas_deformed : mesh.m_blas;

                D3D12_RAYTRACING_INSTANCE_DESC tmp{};
                (float3x4&)tmp.Transform = to_float3x4(inst.m_transform);
                tmp.InstanceID = i;
                tmp.InstanceMask = ~0;
                tmp.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
                tmp.AccelerationStructure = blas->GetGPUVirtualAddress();
                instance_descs[num_descs++] = tmp;
            }
            td.instance_desc->Unmap(0, nullptr);

            inputs.NumDescs = num_descs;
            inputs.InstanceDescs = td.instance_desc->GetGPUVirtualAddress();
        }

        // create TLAS
        {
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
            m_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

            // scratch buffer
            ReuseOrExpandBuffer(td.scratch, 1, info.ScratchDataSizeInBytes, 1024 * 64, [this, &scene](size_t size) {
                auto ret = createBuffer(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
                lptSetName(ret, scene.m_name + " TLAS Scratch");
                return ret;
                });

            // TLAS buffer
            bool expanded = ReuseOrExpandBuffer(td.buffer, 1, info.ResultDataMaxSizeInBytes, 1024 * 256, [this, &scene](size_t size) {
                auto ret = createBuffer(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);
                lptSetName(ret, scene.m_name + " TLAS");
                return ret;
                });
            if (expanded) {
                // SRV
                D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
                srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srv_desc.RaytracingAccelerationStructure.Location = td.buffer->GetGPUVirtualAddress();
                m_device->CreateShaderResourceView(nullptr, &srv_desc, td.srv.hcpu);
            }

            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC as_desc{};
            as_desc.DestAccelerationStructureData = td.buffer->GetGPUVirtualAddress();
            as_desc.Inputs = inputs;
            if (td.instance_desc)
                as_desc.Inputs.InstanceDescs = td.instance_desc->GetGPUVirtualAddress();
            if (td.scratch)
                as_desc.ScratchAccelerationStructureData = td.scratch->GetGPUVirtualAddress();

            // build
            cl_tlas->BuildRaytracingAccelerationStructure(&as_desc, 0, nullptr);
        }

        // add UAV barrier
        {
            D3D12_RESOURCE_BARRIER uav_barrier{};
            uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            uav_barrier.UAV.pResource = td.buffer;
            cl_tlas->ResourceBarrier(1, &uav_barrier);
        }
    }
    lptTimestampQuery(scene.m_timestamp, cl_tlas, "Building TLAS end");
    cl_tlas->Close();
    scene.m_fv_tlas = submitDirectCommandList(cl_tlas, m_fv_blas);


    // setup per-instance data
    if (needs_build_tlas) {
        size_t stride = sizeof(InstanceDataDXR);
        bool expanded = ReuseOrExpandBuffer(scene.m_instance_data, stride, instance_count, 4096, [this, &scene](size_t size) {
            auto ret = createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
            lptSetName(ret, scene.m_name + " Instance Data");
            return ret;
            });
        if (expanded) {
            auto capacity = scene.m_instance_data->GetDesc().Width;
            D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            desc.Buffer.FirstElement = 0;
            desc.Buffer.NumElements = UINT(capacity / stride);
            desc.Buffer.StructureByteStride = UINT(stride);
            desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            m_device->CreateShaderResourceView(scene.m_instance_data, &desc, scene.m_instance_data_srv.hcpu);
        }

        InstanceDataDXR* dst;
        if (SUCCEEDED(scene.m_instance_data->Map(0, nullptr, (void**)&dst))) {
            for (auto& inst : scene.m_instances) {
                InstanceDataDXR tmp{};
                tmp.instance_flags = inst->m_instance_flags;
                *dst++ = tmp;
            }
            scene.m_instance_data->Unmap(0, nullptr);
        }
    }
}


void ContextDXR::renderBegin(IScene* v)
{
    auto& scene = *static_cast<SceneDXR*>(v);

    // initialize desc heap
    if (!scene.m_desc_heap) {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.NumDescriptors = 32;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&scene.m_desc_heap));
        lptSetName(scene.m_desc_heap, scene.m_name + " Desc Heap");

        auto handle_allocator = DescriptorHeapAllocatorDXR(m_device, scene.m_desc_heap);
        scene.m_render_target_uav = handle_allocator.allocate();
        scene.m_tlas_data.srv = handle_allocator.allocate();
        //scene.m_vertex_buffer_srv = handle_allocator.allocate();
        //scene.m_material_data_srv = handle_allocator.allocate();
        scene.m_instance_data_srv = handle_allocator.allocate();
        scene.m_scene_data_cbv = handle_allocator.allocate();
    }

    // initialize scene constant buffer
    if (!scene.m_scene_data) {
        // size of constant buffer must be multiple of 256
        int cb_size = align_to(256, sizeof(SceneData));
        scene.m_scene_data = createBuffer(cb_size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
        lptSetName(scene.m_scene_data, scene.m_name + " Scene Data");

        SceneData* dst;
        if (SUCCEEDED(scene.m_scene_data->Map(0, nullptr, (void**)&dst))) {
            *dst = SceneData{};
            scene.m_scene_data->Unmap(0, nullptr);
        }

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc{};
        cbv_desc.BufferLocation = scene.m_scene_data->GetGPUVirtualAddress();
        cbv_desc.SizeInBytes = cb_size;
        m_device->CreateConstantBufferView(&cbv_desc, scene.m_scene_data_cbv.hcpu);
    }

    lptTimestampInitialize(scene.m_timestamp, m_device);
    lptTimestampReset(scene.m_timestamp);
    lptTimestampSetEnable(scene.m_timestamp, GetGlobals().hasDebugFlag(DebugFlag::Timestamp));

    std::swap(scene.m_instances, scene.m_instances_prev);
    scene.m_instances.clear();

    // reset fence values
    scene.m_fv_tlas = scene.m_fv_rays = 0;

    if (m_fv_last_rays != 0) {
        // wait for complete previous renderer's DispatchRays() because there may be dependencies. (e.g. building BLAS)
        m_cmd_queue_direct->Wait(m_fence, m_fv_last_rays);
    }
}

void ContextDXR::renderEnd(IScene* v)
{
    // todo
}

void ContextDXR::frameEnd()
{
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
