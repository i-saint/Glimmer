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
    void deform(ID3D12GraphicsCommandList4Ptr& cl, MeshInstanceDXR& inst);

private:
    ContextDXR* m_context = nullptr;

    ID3D12RootSignaturePtr m_rootsig;
    ID3D12PipelineStatePtr m_pipeline_state;
    CommandListManagerDXRPtr m_clm_deform;
};
using DeformerDXRPtr = std::shared_ptr<DeformerDXR>;

} // namespace gpt
#endif // _WIN32
