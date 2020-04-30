#pragma once

#ifdef _WIN32
namespace gpt {

class ContextDXR;

class DeformerDXR
{
public:
    DeformerDXR(ContextDXR* ctx);
    ~DeformerDXR();
    bool valid() const;
    int deform();
    void reset();

public:
    ContextDXR* m_context = nullptr;

    ID3D12RootSignaturePtr m_rootsig;
    ID3D12PipelineStatePtr m_pipeline_state;
    ID3D12CommandAllocatorPtr m_cmd_allocator;
    ID3D12GraphicsCommandList4Ptr m_cmd_list;
};
using DeformerDXRPtr = std::shared_ptr<DeformerDXR>;

} // namespace gpt
#endif // _WIN32
