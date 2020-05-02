#include "pch.h"
#ifdef _WIN32
#include "Foundation/gptLog.h"
#include "gptEntityDXR.h"
#include "gptContextDXR.h"

namespace gpt {

void RenderTargetDXR::WindowCallback::onResize(int w, int h)
{
    m_self->m_width = w;
    m_self->m_height = h;
    m_self->markDirty(DirtyFlag::RenderTarget);
}

RenderTargetDXR::RenderTargetDXR(int width, int height, Format format)
    : super(width, height, format)
{
}

RenderTargetDXR::RenderTargetDXR(IWindow* window, Format format)
    : super(window->getWidth(), window->getHeight(), format)
{
    m_callback.m_self = this;

    m_window = base_t(window);
    m_window->addCallback(&m_callback);
}

bool RenderTargetDXR::readback(void* dst)
{
    if (m_readback_enabled && m_buf_readback && dst) {
        m_context->readbackTexture(dst, m_buf_readback, m_width, m_height, GetDXGIFormatTyped(m_format));
        return true;
    }
    else {
        return false;
    }
}

void* RenderTargetDXR::getDeviceObject() const
{
    return m_frame_buffer;
}

void RenderTargetDXR::updateResources()
{
    ContextDXR* ctx = m_context;

    if (!m_uav_frame_buffer) {
        auto& desc_alloc = ctx->m_desc_alloc_srv;
        m_uav_frame_buffer = desc_alloc.allocate();
        m_uav_accum_buffer = desc_alloc.allocate();
    }

    if (m_window) {
        bool update_srvs = false;
        if (!m_swapchain) {
            m_swapchain = std::make_shared<SwapchainDXR>(m_context, m_window, DXGI_FORMAT_R8G8B8A8_UNORM);
        }
        else if (isDirty(DirtyFlag::RenderTarget)) {
            // window resized
            m_context->wait();
            m_swapchain->resize(m_width, m_height);
            m_frame_buffer = nullptr;
            m_accum_buffer = nullptr;
            m_buf_readback = nullptr;
        }
    }

    if (!m_frame_buffer) {
        m_frame_buffer = ctx->createTexture(m_width, m_height, GetDXGIFormatTyped(m_format));
        ctx->createTextureUAV(m_uav_frame_buffer, m_frame_buffer);
        gptSetName(m_frame_buffer, m_name + " Frame Buffer");
    }
    if (!m_accum_buffer) {
        m_accum_buffer = ctx->createTexture(m_width, m_height, DXGI_FORMAT_R32G32B32A32_TYPELESS);
        ctx->createTextureUAV(m_uav_accum_buffer, m_accum_buffer);
        gptSetName(m_accum_buffer, m_name + " Accum Buffer");
    }
    if (m_readback_enabled && !m_buf_readback) {
        m_buf_readback = ctx->createTextureReadbackBuffer(m_width, m_height, GetDXGIFormatTyped(m_format));
        gptSetName(m_buf_readback, m_name + " Readback Buffer");
    }
}

void RenderTargetDXR::readback()
{
    ContextDXR* ctx = m_context;
    if (m_swapchain) {
        auto buffer = m_swapchain->getBuffer(m_swapchain->getCurrentBufferIndex());
        ctx->addResourceBarrier(buffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
        ctx->copyResource(buffer, m_frame_buffer);
        ctx->addResourceBarrier(buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);

    }
    if (m_readback_enabled) {
        ctx->copyTexture(m_buf_readback, m_frame_buffer, m_width, m_height, GetDXGIFormatTyped(m_format));
    }
}

void RenderTargetDXR::present()
{
    if (m_swapchain) {
        m_swapchain->present();
    }
}


TextureDXR::TextureDXR(int width, int height, Format format)
    : super(width, height, format)
{
}

void* TextureDXR::getDeviceObject() const
{
    return m_texture;
}

void TextureDXR::updateResources()
{
    ContextDXR* ctx = m_context;
    if (!m_texture) {
        auto format = GetDXGIFormatTyped(m_format);
        m_texture = ctx->createTexture(m_width, m_height, format);
        m_buf_upload = ctx->createTextureUploadBuffer(m_width, m_height, format);
        gptSetName(m_texture, m_name + " Texture");
        gptSetName(m_buf_upload, m_name + " Upload Buffer");

        m_srv = ctx->m_srv_textures + size_t(m_id);
        ctx->createTextureSRV(m_srv, m_texture);
    }
    if (isDirty(DirtyFlag::TextureData)) {
        ctx->uploadTexture(m_texture, m_buf_upload, m_data.cdata(), m_width, m_height, GetDXGIFormatTyped(m_format));
        ctx->addResourceBarrier(m_texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
    }
}


MeshDXR::MeshDXR()
{
}

MeshDXR::~MeshDXR()
{
    m_context->m_deform_mesh_indices.free(m_data.deform_id);
}

void MeshDXR::prepare()
{
    m_blas_updated = false;
}

void MeshDXR::updateResources()
{
    if (m_points.empty() || m_indices.empty())
        return;

    ContextDXR* ctx = m_context;
    auto& cl_blas = ctx->m_cl;

    // update index buffer
    if (isDirty(DirtyFlag::Indices)) {
        bool allocated = ctx->updateBuffer(m_buf_indices, m_buf_indices_staging, m_indices.cdata(), m_indices.size() * sizeof(int));
        if (allocated) {
            gptSetName(m_buf_indices, m_name + " Indices");
        }
    }

    // update vertex buffer
    if (isDirty(DirtyFlag::Vertices)) {
        bool allocated = ctx->updateBuffer(m_buf_vertices, m_buf_vertices_staging, getVertexCount() * sizeof(vertex_t), [this](vertex_t* dst) {
            exportVertices(dst);
        });
        if (allocated) {
            gptSetName(m_buf_vertices, m_name + " Vertices");
            m_srv_vertices = ctx->m_srv_vertices + size_t(m_id);
            ctx->createBufferSRV(m_srv_vertices, m_buf_vertices, sizeof(vertex_t));
        }
    }

    // update face buffer
    if (isDirty(DirtyFlag::Shape)) {
        bool allocated = ctx->updateBuffer(m_buf_faces, m_buf_faces_staging, getFaceCount() * sizeof(face_t), [this](face_t* dst) {
            exportFaces(dst);
        });
        if (allocated) {
            gptSetName(m_buf_faces, m_name + " Faces");
            m_srv_faces = ctx->m_srv_faces + size_t(m_id);
            ctx->createBufferSRV(m_srv_faces, m_buf_faces, sizeof(face_t));
        }
    }

    if ((isDirty(DirtyFlag::Joints) || isDirty(DirtyFlag::Blendshape)) && m_data.deform_id == -1) {
        m_data.deform_id = ctx->m_deform_mesh_indices.allocate();

        auto base = ctx->m_srv_deform_meshes + size_t(m_data.deform_id * 6);
        m_srv_joint_counts  = base + size_t(0);
        m_srv_joint_weights = base + size_t(1);
        m_srv_bs            = base + size_t(2);
        m_srv_bs_frames     = base + size_t(3);
        m_srv_bs_delta      = base + size_t(4);
        m_srv_mesh          = base + size_t(5);

        ctx->createBufferSRV(m_srv_mesh, ctx->m_buf_meshes, sizeof(MeshData), sizeof(MeshData) * m_id);
    }

    // update joint buffers
    if (isDirty(DirtyFlag::Joints)) {
        if (hasJoints()) {
            bool allocated;
            allocated = ctx->updateBuffer(m_buf_joint_counts, m_buf_joint_counts_staging, getVertexCount() * sizeof(JointCount), [this](JointCount* dst) {
                exportJointCounts(dst);
            });
            if (allocated) {
                gptSetName(m_buf_joint_counts, m_name + " Joint Counts");
                ctx->createBufferSRV(m_srv_joint_counts, m_buf_joint_counts, sizeof(JointCount));
            }

            allocated = ctx->updateBuffer(m_buf_joint_weights, m_buf_joint_weights_staging, getJointWeightCount() * sizeof(JointWeight), [this](JointWeight* dst) {
                exportJointWeights(dst);
            });
            if (allocated) {
                gptSetName(m_buf_joint_weights, m_name + " Joint Weights");
                ctx->createBufferSRV(m_srv_joint_weights, m_buf_joint_weights, sizeof(JointWeight));
            }
        }
        else if (m_buf_joint_counts) {
            m_buf_joint_counts = m_buf_joint_counts_staging = nullptr;
            m_buf_joint_weights = m_buf_joint_weights_staging = nullptr;
        }
    }

    // update blendshape buffers
    if (isDirty(DirtyFlag::Blendshape)) {
        if (hasBlendshapes()) {
            bool allocated;
            allocated = ctx->updateBuffer(m_buf_bs, m_buf_bs_staging, getBlendshapeCount() * sizeof(BlendshapeData), [this](BlendshapeData* dst) {
                exportBlendshapes(dst);
            });
            if (allocated) {
                gptSetName(m_buf_bs, m_name + " Blendshape");
                ctx->createBufferSRV(m_srv_bs, m_buf_bs, sizeof(BlendshapeData));
            }

            allocated = ctx->updateBuffer(m_buf_bs_frames, m_buf_bs_frames_staging, getBlendshapeFrameCount() * sizeof(BlendshapeFrameData), [this](BlendshapeFrameData* dst) {
                exportBlendshapeFrames(dst);
            });
            if (allocated) {
                gptSetName(m_buf_bs_frames, m_name + " Blendshape Frames");
                ctx->createBufferSRV(m_srv_bs_frames, m_buf_bs_frames, sizeof(BlendshapeFrameData));
            }

            allocated = ctx->updateBuffer(m_buf_bs_delta, m_buf_bs_delta_staging, getBlendshapeFrameCount() * getVertexCount() * sizeof(vertex_t), [this](vertex_t* dst) {
                exportBlendshapeDelta(dst);
            });
            if (allocated) {
                gptSetName(m_buf_bs_delta, m_name + " Blendshape Delta");
                ctx->createBufferSRV(m_srv_bs_delta, m_buf_bs_delta, sizeof(vertex_t));
            }
        }
        else if (m_buf_bs) {
            m_buf_bs = m_buf_bs_staging = nullptr;
            m_buf_bs_frames = m_buf_bs_frames_staging = nullptr;
            m_buf_bs_delta = m_buf_bs_delta_staging = nullptr;
        }
    }
}

void MeshDXR::updateBLAS()
{
    bool needs_update_blas = m_buf_vertices && m_buf_indices && (!m_blas || isDirty(DirtyFlag::Shape));
    if (!needs_update_blas)
        return;

    ContextDXR* ctx = m_context;
    auto& cl_blas = ctx->m_cl;

    // BLAS for non-deformable meshes

    bool perform_update = m_blas != nullptr;

    D3D12_RAYTRACING_GEOMETRY_DESC geom_desc{};
    geom_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geom_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    geom_desc.Triangles.VertexBuffer.StartAddress = m_buf_vertices->GetGPUVirtualAddress();
    geom_desc.Triangles.VertexBuffer.StrideInBytes = sizeof(vertex_t);
    geom_desc.Triangles.VertexCount = (UINT)getVertexCount();
    geom_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

    geom_desc.Triangles.IndexBuffer = m_buf_indices->GetGPUVirtualAddress();
    geom_desc.Triangles.IndexCount = (UINT)getIndexCount();
    geom_desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    if (isDynamic()) {
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

    if (!m_blas || isDirty(DirtyFlag::Indices)) {
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
        ctx->m_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

        m_blas_scratch = ctx->createBuffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
        m_blas = ctx->createBuffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);
        gptSetName(m_blas_scratch, m_name + " BLAS Scratch");
        gptSetName(m_blas, m_name + " BLAS");
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC as_desc{};
    as_desc.Inputs = inputs;
    if (perform_update)
        as_desc.SourceAccelerationStructureData = m_blas->GetGPUVirtualAddress();
    as_desc.DestAccelerationStructureData = m_blas->GetGPUVirtualAddress();
    as_desc.ScratchAccelerationStructureData = m_blas_scratch->GetGPUVirtualAddress();

    cl_blas->BuildRaytracingAccelerationStructure(&as_desc, 0, nullptr);
    m_blas_updated = true;
}

void MeshDXR::clearBLAS()
{
    m_blas = nullptr;
    m_blas_scratch = nullptr;
}


MeshInstanceDXR::MeshInstanceDXR(IMesh* v)
    : super(v)
{
}

MeshInstanceDXR::~MeshInstanceDXR()
{
    m_context->m_deform_instance_indices.free(m_data.deform_id);
}

void MeshInstanceDXR::prepare()
{
    m_blas_updated = false;
}

void MeshInstanceDXR::updateResources()
{
    ContextDXR* ctx = m_context;
    auto& mesh = dxr_t(*m_mesh);
    if (mesh.hasJoints() || mesh.hasBlendshapes()) {
        if (m_data.deform_id == -1) {
            m_data.deform_id = m_context->m_deform_instance_indices.allocate();

            m_srv_vertices = ctx->m_srv_vertices_d + size_t(m_data.deform_id);

            auto base = ctx->m_srv_deform_instances + size_t(m_data.deform_id * 3);
            m_uav_vertices      = base + size_t(0);
            m_srv_joint_matrices= base + size_t(1);
            m_srv_bs_weights    = base + size_t(2);
        }

        // vertex buffer for deformation
        if (mesh.isDirty(DirtyFlag::Vertices)) {
            m_buf_vertices = ctx->createBuffer(mesh.getVertexCount() * sizeof(vertex_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
            gptSetName(m_buf_vertices, m_name + " Deformed Vertices");
            ctx->createBufferSRV(m_srv_vertices, m_buf_vertices, sizeof(vertex_t));
            ctx->createBufferUAV(m_uav_vertices, m_buf_vertices, sizeof(vertex_t));
        }

        if (isDirty(DirtyFlag::Joints)) {
            bool allocated = ctx->updateBuffer(m_buf_joint_matrices, m_buf_joint_matrices_staging, mesh.getJointCount() * sizeof(float4x4), [this](float4x4* dst) {
                exportJointMatrices(dst);
            });
            if (allocated) {
                gptSetName(m_buf_joint_matrices, m_name + " Joint Matrices");
                ctx->createBufferSRV(m_srv_joint_matrices, m_buf_joint_matrices, sizeof(float4x4));
            }
        }

        if (isDirty(DirtyFlag::Blendshape)) {
            bool allocated = ctx->updateBuffer(m_buf_bs_weights, m_buf_bs_weights_staging, mesh.getBlendshapeCount() * sizeof(float), [this](float* dst) {
                exportBlendshapeWeights(dst);
            });
            if (allocated) {
                gptSetName(m_buf_bs_weights, m_name + " Blendshape Weights");
                ctx->createBufferSRV(m_srv_bs_weights, m_buf_bs_weights, sizeof(float));
            }
        }
    }
    else {
        if (m_buf_vertices) {
            m_buf_vertices = nullptr;
            m_buf_joint_matrices = m_buf_joint_matrices_staging = nullptr;
            m_buf_bs_weights = m_buf_bs_weights_staging = nullptr;
            m_blas = m_blas_scratch = nullptr;
        }
    }
}

void MeshInstanceDXR::updateBLAS()
{
    ContextDXR* ctx = m_context;
    auto& cl_blas = ctx->m_cl;
    auto& mesh = dxr_t(*m_mesh);

    if (m_buf_vertices && (!m_blas || isDirty(DirtyFlag::Deform))) {
        // BLAS for deformable meshes

        bool perform_update = m_blas != nullptr;

        D3D12_RAYTRACING_GEOMETRY_DESC geom_desc{};
        geom_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geom_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

        geom_desc.Triangles.VertexBuffer.StartAddress = m_buf_vertices->GetGPUVirtualAddress();
        geom_desc.Triangles.VertexBuffer.StrideInBytes = sizeof(vertex_t);
        geom_desc.Triangles.VertexCount = (UINT)mesh.getVertexCount();
        geom_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

        geom_desc.Triangles.IndexBuffer = mesh.m_buf_indices->GetGPUVirtualAddress();
        geom_desc.Triangles.IndexCount = (UINT)mesh.getIndexCount();
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

        if (!m_blas) {
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
            ctx->m_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

            m_blas_scratch = ctx->createBuffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
            m_blas = ctx->createBuffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);
            gptSetName(m_blas_scratch, m_name + " BLAS Scratch");
            gptSetName(m_blas, m_name + " BLAS");
        }

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC as_desc{};
        as_desc.DestAccelerationStructureData = m_blas->GetGPUVirtualAddress();
        as_desc.Inputs = inputs;
        if (perform_update)
            as_desc.SourceAccelerationStructureData = m_blas->GetGPUVirtualAddress();
        as_desc.ScratchAccelerationStructureData = m_blas_scratch->GetGPUVirtualAddress();

        ctx->addResourceBarrier(m_buf_vertices, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        cl_blas->BuildRaytracingAccelerationStructure(&as_desc, 0, nullptr);
        ctx->addResourceBarrier(m_buf_vertices, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        m_blas_updated = true;
    }

    if (!m_blas_updated) {
        if (isDirty(DirtyFlag::Transform) || mesh.m_blas_updated) {
            // transform or mesh are updated. TLAS needs to be updated.
            m_blas_updated = true;
        }
    }
}

void MeshInstanceDXR::clearBLAS()
{
    m_blas_scratch = nullptr;
    m_blas = nullptr;
}


void SceneDXR::updateResources()
{
    if (!m_enabled)
        return;
    incrementFrameCount();

    ContextDXR* ctx = m_context;

    // desc heap
    if (!m_srv_tlas) {
        auto& desc_alloc = ctx->m_desc_alloc_srv;
        m_srv_tlas  = desc_alloc.allocate();
        m_cbv_scene = desc_alloc.allocate();
    }

    // scene constant buffer
    // size of constant buffer must be multiple of 256
    int cb_size = align_to(256, sizeof(SceneData));
    bool allocated = ctx->updateBuffer(m_buf_scene, m_buf_scene_staging, cb_size, [this](SceneData* mapped) {
        *mapped = getData();
    });
    if (allocated) {
        gptSetName(m_buf_scene, m_name + " Scene Buffer");
        ctx->createCBV(m_cbv_scene, m_buf_scene, cb_size);
    }
}

void SceneDXR::updateTLAS()
{
    if (!m_enabled)
        return;

    bool needs_update_tlas = false;
    for (auto& pinst : m_instances) {
        if (dxr_t(*pinst).m_blas_updated) {
            needs_update_tlas = true;
            break;
        }
    }
    if (!needs_update_tlas)
        return;


    ContextDXR* ctx = m_context;
    auto& cl_tlas = ctx->m_cl;

    // get the size of the TLAS buffers
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

    // create instance desc buffer
    ExpandBuffer(m_tlas_instance_desc, sizeof(D3D12_RAYTRACING_INSTANCE_DESC), (UINT)m_instances.size(), 4096, [this, ctx](size_t size) {
        m_tlas_instance_desc = ctx->createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
        gptSetName(m_tlas_instance_desc, m_name + " TLAS Instance Desk");
    });

    // update instance desc
    int active_instance_count = 0;
    Map(m_tlas_instance_desc, [&](D3D12_RAYTRACING_INSTANCE_DESC* instance_descs) {
        D3D12_RAYTRACING_INSTANCE_DESC desc{};
        for (auto& pinst : m_instances) {
            auto& inst = dxr_t(*pinst);
            if (!inst.isEnabled())
                continue;

            auto& mesh = dxr_t(*inst.getMesh());
            auto& blas = inst.m_blas ? inst.m_blas : mesh.m_blas;

            (float3x4&)desc.Transform = to_mat3x4(inst.getData().local_to_world);
            desc.InstanceID = inst.getID();
            desc.InstanceMask = ~0;
            desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
            desc.AccelerationStructure = blas->GetGPUVirtualAddress();
            *instance_descs++ = desc;
            ++active_instance_count;
        }
    });
    inputs.NumDescs = active_instance_count;
    inputs.InstanceDescs = m_tlas_instance_desc->GetGPUVirtualAddress();

    // create TLAS
    {
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
        ctx->m_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

        // scratch buffer
        ExpandBuffer(m_tlas_scratch, 1, info.ScratchDataSizeInBytes, 1024 * 64, [this, ctx](size_t size) {
            m_tlas_scratch = ctx->createBuffer(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
            gptSetName(m_tlas_scratch, m_name + " TLAS Scratch");
        });

        // TLAS buffer
        bool allocated = ExpandBuffer(m_tlas, 1, info.ResultDataMaxSizeInBytes, 1024 * 256, [this, ctx](size_t size) {
            m_tlas = ctx->createBuffer(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);
            gptSetName(m_tlas, m_name + " TLAS");
        });
        if (allocated) {
            // SRV
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.RaytracingAccelerationStructure.Location = m_tlas->GetGPUVirtualAddress();
            ctx->m_device->CreateShaderResourceView(nullptr, &srv_desc, m_srv_tlas.hcpu);
        }

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC as_desc{};
        as_desc.DestAccelerationStructureData = m_tlas->GetGPUVirtualAddress();
        as_desc.Inputs = inputs;
        as_desc.ScratchAccelerationStructureData = m_tlas_scratch->GetGPUVirtualAddress();

        // build
        cl_tlas->BuildRaytracingAccelerationStructure(&as_desc, 0, nullptr);
    }

    // add UAV barrier
    {
        D3D12_RESOURCE_BARRIER uav_barrier{};
        uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uav_barrier.UAV.pResource = m_tlas;
        cl_tlas->ResourceBarrier(1, &uav_barrier);
    }
}

} // namespace gpt
#endif // _WIN32
