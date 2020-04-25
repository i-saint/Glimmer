#include "pch.h"
#ifdef _WIN32
#include "Foundation/lptLog.h"
#include "lptContextDXR.h"
#include "lptDeformerDXR.h"

// shader binaries
#include "lptDeform.hlsl.h"

namespace lpt {

#if 0
enum class DeformFlag : uint32_t
{
    Blendshape = 1,
    Skinning = 2,
};

struct BlendshapeFrame
{
    int delta_offset;
    float weight;
};
struct BlendshapeInfo
{
    int frame_count;
    int frame_offset;
};

struct BoneCount
{
    int weight_count;
    int weight_offset;
};

struct MeshInfo
{
    int deform_flags;
    int vertex_stride; // in element (e.g. 6 if position + normal)
    int2 pad1;
};



DeformerDXR::DeformerDXR(ID3D12Device5Ptr device)
    : m_device(device)
{
    {
        const D3D12_DESCRIPTOR_RANGE ranges[] = {
            { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 8, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
            { D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
        };
        D3D12_ROOT_PARAMETER param{};
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param.DescriptorTable.NumDescriptorRanges = _countof(ranges);
        param.DescriptorTable.pDescriptorRanges = ranges;

        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.NumParameters = 1;
        desc.pParameters = &param;

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
            lptSetName(m_rootsig, L"Deform Rootsig");
        }
    }

    if (m_rootsig) {
        D3D12_COMPUTE_PIPELINE_STATE_DESC psd {};
        psd.pRootSignature = m_rootsig.GetInterfacePtr();
        psd.CS.pShaderBytecode = g_lptDeform;
        psd.CS.BytecodeLength = sizeof(g_lptDeform);

        HRESULT hr = m_device->CreateComputePipelineState(&psd, IID_PPV_ARGS(&m_pipeline_state));
        if (FAILED(hr)) {
            SetErrorLog("CreateComputePipelineState() failed\n");
        }
    }

    if (m_pipeline_state) {
        lptSetName(m_pipeline_state, L"Deform Pipeline State");
        m_clm_deform = std::make_shared<CommandListManagerDXR>(m_device, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_pipeline_state, L"Deforms");
    }
}

DeformerDXR::~DeformerDXR()
{
}

bool DeformerDXR::valid() const
{
    return this && m_device && m_rootsig && m_pipeline_state;
}

bool DeformerDXR::prepare(ContextDXR& rd)
{
    if (!valid())
        return false;

    rd.cl_deform = m_clm_deform->get();
    lptTimestampQuery(rd.timestamp, rd.cl_deform, "Deform begin");
    return true;
}

bool DeformerDXR::deform(ContextDXR& rd, MeshInstanceDXR& inst)
{
    if (!valid() || !inst.m_mesh)
        return false;

    auto& mesh = dxr_t(*inst.m_mesh);

    bool blendshape_updated = inst.isDirty(DirtyFlag::Blendshape);
    bool bone_updated = inst.isDirty(DirtyFlag::Joints);
    if (!blendshape_updated && !bone_updated)
        return false; // no need to deform

    bool clamp_blendshape_weights = rd.hasFlag(RenderFlag::ClampBlendShapeWights) != 0;
    int vertex_count = (int)mesh.m_points.size();
    int blendshape_count = (int)inst.m_blendshape_weights.size();
    int bone_count = (int)inst.m_joint_matrices.size();

    // setup descriptors
    bool update_descriptors = false;
    if (!inst.m_desc_heap) {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.NumDescriptors = 32;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&inst.m_desc_heap));
        lptSetName(inst.m_desc_heap, inst.m_name + " Desc Heap");
        update_descriptors = true;
    }

    // note: there are per-mesh resources (e.g. base vertices) and per-instance ones (e.g. deformed vertices).
    //       these are not 1 on 1. one mesh can have multiple instances.
    auto handle_allocator = DescriptorHeapAllocatorDXR(m_device, inst.m_desc_heap);
    auto hdst_vertices = handle_allocator.allocate();
    auto hbase_vertices = handle_allocator.allocate();
    auto hbs_delta = handle_allocator.allocate();
    auto hbs_frames = handle_allocator.allocate();
    auto hbs_info = handle_allocator.allocate();
    auto hbs_weights = handle_allocator.allocate();
    auto hbone_counts = handle_allocator.allocate();
    auto hbone_weights = handle_allocator.allocate();
    auto hbone_matrices = handle_allocator.allocate();
    auto hmesh_info = handle_allocator.allocate();

    if (!inst.m_buf_points_deformed) {
        // deformed vertices
        inst.m_buf_points_deformed = createBuffer(sizeof(float4) * vertex_count, kDefaultHeapProps, true);
        lptSetName(inst.m_buf_points_deformed, inst.m_name + " Deformed Vertices");
    }
    if (update_descriptors) {
        createUAV(hdst_vertices.hcpu, inst.m_buf_points_deformed, vertex_count, sizeof(float4));
        createSRV(hbase_vertices.hcpu, mesh.m_buf_points, mesh.m_points.size(), 12);
    }

    // blendshape
    if (blendshape_count > 0) {
        int frame_count = 0;
        for (auto& bs : mesh.blendshapes)
            frame_count += (int)bs.frames.size();

        if (!mesh.m_buf_bs_delta) {
            // delta
            mesh.m_buf_bs_delta = createBuffer(sizeof(float4) * vertex_count * frame_count, kUploadHeapProps);
            lptSetName(mesh.m_buf_bs_delta, mesh.m_name + " Blendshape Delta");
            writeBuffer(mesh.m_buf_bs_delta, [&](void *dst_) {
                auto dst = (float4*)dst_;
                for (auto& bs : mesh.blendshapes) {
                    for (auto& frame : bs.frames) {
                        auto& delta = frame.delta;
                        for (int vi = 0; vi < vertex_count; ++vi)
                            *dst++ = to_float4(delta[vi], 0.0f);
                    }
                }
            });

            // frame
            mesh.m_buf_bs_frames = createBuffer(sizeof(BlendshapeFrame) * frame_count, kUploadHeapProps);
            lptSetName(mesh.m_buf_bs_frames, mesh.m_name + " Blendshape Frame");
            writeBuffer(mesh.m_buf_bs_frames, [&](void *dst_) {
                auto dst = (BlendshapeFrame*)dst_;
                int offset = 0;
                for (auto& bs : mesh.blendshapes) {
                    for (auto& frame : bs.frames) {
                        BlendshapeFrame tmp{};
                        tmp.delta_offset = offset;
                        tmp.weight = frame.weight / 100.0f; // 0-100 -> 0.0-1.0
                        *dst++ = tmp;

                        offset += vertex_count;
                    }
                }
            });

            // counts
            mesh.m_buf_bs_info = createBuffer(sizeof(BlendshapeInfo) * blendshape_count, kUploadHeapProps);
            lptSetName(mesh.m_buf_bs_info, mesh.m_name + " Blendshape Counts");
            writeBuffer(mesh.m_buf_bs_info, [&](void *dst_) {
                auto dst = (BlendshapeInfo*)dst_;
                int offset = 0;
                for (auto& bs : mesh.blendshapes) {
                    BlendshapeInfo tmp{};
                    tmp.frame_count = (int)bs.frames.size();
                    tmp.frame_offset = offset;
                    *dst++ = tmp;

                    offset += tmp.frame_count;
                }
            });
        }

        // weights
        {
            if (!inst.m_buf_bs_weights) {
                inst.m_buf_bs_weights = createBuffer(sizeof(float) * blendshape_count, kUploadHeapProps);
                lptSetName(inst.m_buf_bs_weights, inst.m_name + " Blendshape Weights");
            }
            // update on every frame
            writeBuffer(inst.m_buf_bs_weights, [&](void *dst_) {
                auto dst = (float*)dst_;
                for (int bsi = 0; bsi < blendshape_count; ++bsi) {
                    float weight = inst.m_blendshape_weights[bsi];
                    if (clamp_blendshape_weights)
                        weight = clamp(weight, 0.0f, mesh.blendshapes[bsi].frames.back().weight);
                    *dst++ = weight / 100.0f; // 0-100 -> 0.0-1.0;
                }
            });
        }

        if (update_descriptors) {
            createSRV(hbs_delta.hcpu, mesh.m_buf_bs_delta, vertex_count * frame_count, sizeof(float4));
            createSRV(hbs_frames.hcpu, mesh.m_buf_bs_frames, frame_count, sizeof(BlendshapeFrame));
            createSRV(hbs_info.hcpu, mesh.m_buf_bs_info, blendshape_count, sizeof(BlendshapeInfo));
            createSRV(hbs_weights.hcpu, inst.m_buf_bs_weights, blendshape_count, sizeof(float));
        }
    }

    // skinning 
    if (bone_count > 0) {
        // bone counts & weights
        if (!mesh.m_buf_joint_counts) {
            mesh.m_buf_joint_counts = createBuffer(sizeof(BoneCount) * vertex_count, kUploadHeapProps);
            lptSetName(mesh.m_buf_joint_counts, mesh.m_name + " Bone Counts");

            int weight_offset = 0;
            writeBuffer(mesh.m_buf_joint_counts, [&](void *dst_) {
                auto dst = (BoneCount*)dst_;
                for (int vi = 0; vi < vertex_count; ++vi) {
                    int n = mesh.m_joint_counts[vi];
                    *dst++ = { n, weight_offset };
                    weight_offset += n;
                }
            });

            const int weight_count = weight_offset;
            mesh.m_buf_joint_weights = createBuffer(sizeof(JointWeight) * weight_count, kUploadHeapProps);
            lptSetName(mesh.m_buf_joint_weights, mesh.m_name + " Bone Weights");
            writeBuffer(mesh.m_buf_joint_weights, [&](void *dst_) {
                auto dst = (JointWeight*)dst_;
                for (int wi = 0; wi < weight_count; ++wi) {
                    auto& w1 = mesh.m_joint_weights[wi];
                    *dst++ = { w1.weight, w1.index };
                }
            });
        }

        // bone matrices
        {
            if (!inst.m_buf_joint_matrices) {
                inst.m_buf_joint_matrices = createBuffer(sizeof(float4x4) * bone_count, kUploadHeapProps);
                lptSetName(inst.m_buf_joint_matrices, inst.m_name + " Bone Matrices");
            }
            // update on every frame
            writeBuffer(inst.m_buf_joint_matrices, [&](void *dst_) {
                auto dst = (float4x4*)dst_;

                // note:
                // object space skinning is recommended for better BLAS building. ( http://intro-to-dxr.cwyman.org/presentations/IntroDXR_RaytracingAPI.pdf )
                // so, try to convert bone matrices to root bone space.
                // on skinned meshes, inst.transform is root bone's transform or identity if root bone is not assigned.
                // both cases work, but identity matrix means world space skinning that is not optimal.
                auto iroot = invert(inst.m_transform);
                for (int bi = 0; bi < bone_count; ++bi)
                    *dst++ = mesh.m_joint_bindposes[bi] * inst.m_joint_matrices[bi] * iroot;
            });
        }

        if (update_descriptors) {
            int weight_count = (int)mesh.m_joint_weights.size();
            createSRV(hbone_counts.hcpu, mesh.m_buf_joint_counts, vertex_count, sizeof(BoneCount));
            createSRV(hbone_weights.hcpu, mesh.m_buf_joint_weights, weight_count, sizeof(JointWeight));
            createSRV(hbone_matrices.hcpu, inst.m_buf_joint_matrices, bone_count, sizeof(float4x4));
        }
    }

    // mesh info
    int mesh_info_size = align_to(256, sizeof(MeshInfo));
    if (!mesh.m_buf_mesh_info) {
        mesh.m_buf_mesh_info = createBuffer(mesh_info_size, kUploadHeapProps);
        lptSetName(mesh.m_buf_mesh_info, mesh.m_name + " Mesh Info");
        writeBuffer(mesh.m_buf_mesh_info, [&](void *dst_) {
            MeshInfo info{};
            info.vertex_stride = 12;
            info.deform_flags = 0;
            if (blendshape_count > 0)
                info.deform_flags |= (int)DeformFlag::Blendshape;
            if (bone_count > 0)
                info.deform_flags |= (int)DeformFlag::Skinning;

            *(MeshInfo*)dst_ = info;
        });
    }
    if (update_descriptors) {
        createCBV(hmesh_info.hcpu, mesh.m_buf_mesh_info, mesh_info_size);
    }

    {
        auto& cl = rd.cl_deform;
        cl->SetComputeRootSignature(m_rootsig);

        ID3D12DescriptorHeap* heaps[] = { inst.m_desc_heap };
        cl->SetDescriptorHeaps(_countof(heaps), heaps);
        cl->SetComputeRootDescriptorTable(0, inst.m_desc_heap->GetGPUDescriptorHandleForHeapStart());
        cl->Dispatch((UINT)mesh.m_points.size(), 1, 1);
    }

    return true;
}

uint64_t DeformerDXR::flush(ContextDXR& rd)
{
    if (!valid() || !rd.cl_deform)
        return 0;

    auto cq = getComputeQueue();
    lptTimestampQuery(rd.timestamp, rd.cl_deform, "Deform end");
    rd.cl_deform->Close();
    auto ret = GfxContextDXR::getInstance()->submitComputeCommandList(rd.cl_deform, rd.fv_translate);
    rd.cl_deform = nullptr;
    return ret;
}

bool DeformerDXR::reset()
{
    if (m_clm_deform) {
        m_clm_deform->reset();
        return true;
    }
    return false;
}


void DeformerDXR::createSRV(D3D12_CPU_DESCRIPTOR_HANDLE dst, ID3D12Resource *res, int num_elements, int stride)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Buffer.FirstElement = 0;
    desc.Buffer.NumElements = num_elements;
    desc.Buffer.StructureByteStride = stride;
    desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    m_device->CreateShaderResourceView(res, &desc, dst);
}

void DeformerDXR::createUAV(D3D12_CPU_DESCRIPTOR_HANDLE dst, ID3D12Resource *res, int num_elements, int stride)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    desc.Buffer.FirstElement = 0;
    desc.Buffer.NumElements = num_elements;
    desc.Buffer.StructureByteStride = stride;
    desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    m_device->CreateUnorderedAccessView(res, nullptr, &desc, dst);
}

void DeformerDXR::createCBV(D3D12_CPU_DESCRIPTOR_HANDLE dst, ID3D12Resource *res, int size)
{
    D3D12_CONSTANT_BUFFER_VIEW_DESC desc{};
    desc.BufferLocation = res->GetGPUVirtualAddress();
    desc.SizeInBytes = size;
    m_device->CreateConstantBufferView(&desc, dst);
}


ID3D12ResourcePtr DeformerDXR::createBuffer(int size, const D3D12_HEAP_PROPERTIES& heap_props, bool uav)
{
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = uav ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
    D3D12_RESOURCE_STATES state = uav ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_GENERIC_READ;

    ID3D12ResourcePtr ret;
    auto hr = m_device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&ret));
    if (FAILED(hr)) {
        SetErrorLog("CreateCommittedResource() failed\n");
    }
    return ret;
}

ID3D12CommandQueuePtr DeformerDXR::getComputeQueue()
{
    return GfxContextDXR::getInstance()->getComputeQueue();
}
ID3D12FencePtr DeformerDXR::getFence()
{
    return GfxContextDXR::getInstance()->getFence();
}
uint64_t DeformerDXR::incrementFenceValue()
{
    return GfxContextDXR::getInstance()->incrementFenceValue();
}

template<class Body>
bool DeformerDXR::writeBuffer(ID3D12Resource *res, const Body& body)
{
    void *data;
    auto hr = res->Map(0, nullptr, &data);
    if (SUCCEEDED(hr)) {
        body(data);
        res->Unmap(0, nullptr);
        return true;
    }
    else {
        SetErrorLog("Map() failed\n");
    }
    return false;
}

#endif
} // namespace lpt
#endif // _WIN32
