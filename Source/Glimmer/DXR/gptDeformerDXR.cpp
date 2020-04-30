#include "pch.h"
#ifdef _WIN32
#include "Foundation/gptLog.h"
#include "gptContextDXR.h"
#include "gptDeformerDXR.h"

// shader binaries
#include "gptDeform.hlsl.h"

namespace gpt {




DeformerDXR::DeformerDXR(ContextDXR* ctx)
    : m_context(ctx)
{
    auto& device = ctx->m_device;
    {
        // per-instance data
        D3D12_DESCRIPTOR_RANGE ranges0[] = {
            { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
        };

        // per-mesh data
        D3D12_DESCRIPTOR_RANGE ranges1[] = {
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 1, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
        };
        D3D12_DESCRIPTOR_RANGE ranges2[] = {
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, 1, 1, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
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
            hr = device->CreateRootSignature(0, sig_blob->GetBufferPointer(), sig_blob->GetBufferSize(), IID_PPV_ARGS(&m_rootsig));
            if (FAILED(hr)) {
                SetErrorLog("CreateRootSignature() failed\n");
            }
        }
        if (m_rootsig) {
            gptSetName(m_rootsig, L"Deform Rootsig");
        }
    }

    if (m_rootsig) {
        D3D12_COMPUTE_PIPELINE_STATE_DESC psd {};
        psd.pRootSignature = m_rootsig.GetInterfacePtr();
        psd.CS.pShaderBytecode = g_gptDeform;
        psd.CS.BytecodeLength = sizeof(g_gptDeform);

        HRESULT hr = device->CreateComputePipelineState(&psd, IID_PPV_ARGS(&m_pipeline_state));
        if (FAILED(hr)) {
            SetErrorLog("CreateComputePipelineState() failed\n");
        }
    }

    if (m_pipeline_state) {
        gptSetName(m_pipeline_state, L"Deform Pipeline State");

        device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_cmd_allocator));
        device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_cmd_allocator, m_pipeline_state, IID_PPV_ARGS(&m_cmd_list));
        gptSetName(m_cmd_allocator, L"Deform Command Allocator");
        gptSetName(m_cmd_list, L"Deform Command List");
    }
}

DeformerDXR::~DeformerDXR()
{
}

bool DeformerDXR::valid() const
{
    return m_rootsig && m_pipeline_state;
}

int DeformerDXR::deform()
{
    auto ctx = m_context;
    auto& cl = m_cmd_list;

    ID3D12DescriptorHeap* heaps[] = { ctx->m_desc_heap };
    cl->SetDescriptorHeaps(_countof(heaps), heaps);
    cl->SetComputeRootSignature(m_rootsig);

    int ret = 0;
    each_ref(ctx->m_mesh_instances, [&](auto& inst) {
        auto& mesh = dxr_t(*inst.m_mesh);
        if (mesh.hasJoints() || mesh.hasBlendshapes()) {
            cl->SetComputeRootDescriptorTable(0, inst.m_uav_vertices.hgpu);
            cl->SetComputeRootDescriptorTable(1, mesh.m_srv_vertices.hgpu);
            cl->SetComputeRootDescriptorTable(2, mesh.m_srv_joint_counts.hgpu);
            cl->Dispatch((UINT)mesh.getVertexCount(), 1, 1);
            ++ret;
        }
    });
    return ret;
}

void DeformerDXR::reset()
{
    m_cmd_allocator->Reset();
    m_cmd_list->Reset(m_cmd_allocator, m_pipeline_state);
}

} // namespace gpt
#endif // _WIN32
