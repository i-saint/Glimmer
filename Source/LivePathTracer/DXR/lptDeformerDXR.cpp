#include "pch.h"
#ifdef _WIN32
#include "Foundation/lptLog.h"
#include "lptContextDXR.h"
#include "lptDeformerDXR.h"

// shader binaries
#include "lptDeform.hlsl.h"

namespace lpt {




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
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, 0, 1, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
            { D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 1, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
        };

        D3D12_ROOT_PARAMETER params[2]{};
        auto append = [&params](const int i, auto& range) {
            params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            params[i].DescriptorTable.NumDescriptorRanges = _countof(range);
            params[i].DescriptorTable.pDescriptorRanges = range;
        };
#define Append(I) static_assert(I < _countof(params), "param size exceeded"); append(I, ranges##I)
        Append(0);
        Append(1);
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
            lptSetName(m_rootsig, L"Deform Rootsig");
        }
    }

    if (m_rootsig) {
        D3D12_COMPUTE_PIPELINE_STATE_DESC psd {};
        psd.pRootSignature = m_rootsig.GetInterfacePtr();
        psd.CS.pShaderBytecode = g_lptDeform;
        psd.CS.BytecodeLength = sizeof(g_lptDeform);

        HRESULT hr = device->CreateComputePipelineState(&psd, IID_PPV_ARGS(&m_pipeline_state));
        if (FAILED(hr)) {
            SetErrorLog("CreateComputePipelineState() failed\n");
        }
    }

    if (m_pipeline_state) {
        lptSetName(m_pipeline_state, L"Deform Pipeline State");
        m_clm_deform = std::make_shared<CommandListManagerDXR>(device, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_pipeline_state, L"Deforms");
    }
}

DeformerDXR::~DeformerDXR()
{
}

bool DeformerDXR::valid() const
{
    return m_rootsig && m_pipeline_state;
}

void DeformerDXR::deform(ID3D12GraphicsCommandList4Ptr& cl, MeshInstanceDXR& inst)
{
    auto ctx = m_context;
    auto& mesh = dxr_t(*inst.m_mesh);

    ID3D12DescriptorHeap* heaps[] = { ctx->m_desc_heap };
    cl->SetDescriptorHeaps(_countof(heaps), heaps);
    cl->SetComputeRootSignature(m_rootsig);
    cl->SetComputeRootDescriptorTable(0, inst.m_uav_vertices.hgpu);
    cl->SetComputeRootDescriptorTable(1, mesh.m_srv_vertices.hgpu);
    cl->Dispatch((UINT)mesh.getVertexCount(), 1, 1);
}

} // namespace lpt
#endif // _WIN32
