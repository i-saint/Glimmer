#pragma once
#ifdef _WIN32
#include "Foundation/gptUtils.h"
#include "gptEntityDXR.h"
#include "gptDeformerDXR.h"

namespace gpt {

class ContextDXR : public DXREntity<Context>
{
using super = DXREntity<Context>;
public:
    ContextDXR();
    ~ContextDXR();

    ICameraPtr       createCamera() override;
    ILightPtr        createLight() override;
    IRenderTargetPtr createRenderTarget(int width, int height, Format format) override;
    IRenderTargetPtr createRenderTarget(IWindow* window, Format format) override;
    ITexturePtr      createTexture(int width, int height, Format format) override;
    IMaterialPtr     createMaterial() override;
    IMeshPtr         createMesh() override;
    IMeshInstancePtr createMeshInstance(IMesh* v) override;
    IScenePtr        createScene() override;

    void render() override;
    void finish() override;
    void* getDevice() override;
    const char* getDeviceName() override;
    const char* getTimestampLog() override;

    bool valid() const;
    void clear();
    bool checkError();
    bool initializeDevice();
    void prepare();
    void updateResources();
    void deform();
    void updateBLAS();
    void updateTLAS();
    void dispatchRays();
    void wait();

    uint64_t incrementFenceValue();

    ID3D12ResourcePtr createBuffer(uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, const D3D12_HEAP_PROPERTIES& heap_props);
    ID3D12ResourcePtr createBuffer(uint64_t size);
    ID3D12ResourcePtr createUploadBuffer(uint64_t size);

    ID3D12ResourcePtr createTexture(int width, int height, DXGI_FORMAT format);
    ID3D12ResourcePtr createTextureUploadBuffer(int width, int height, DXGI_FORMAT format);
    ID3D12ResourcePtr createTextureReadbackBuffer(int width, int height, DXGI_FORMAT format);


    uint64_t submit(uint64_t preceding_fv = 0);
    uint64_t submit(ID3D12CommandQueuePtr& queue, ID3D12GraphicsCommandList4Ptr& cl, uint64_t preceding_fv = 0);
    void addResourceBarrier(ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after);

    template<class Body>
    void writeBuffer(ID3D12Resource* dst, ID3D12Resource* staging, UINT64 size, const Body& src);
    void uploadBuffer(ID3D12Resource* dst, ID3D12Resource* staging, const void* src, UINT64 size);
    void copyBuffer(ID3D12Resource* dst, ID3D12Resource* src, UINT64 size);
    void readbackBuffer(void* dst, ID3D12Resource* staging, UINT64 size);

    template<class Body>
    bool updateBuffer(ID3D12ResourcePtr& dst, ID3D12ResourcePtr& staging, size_t size, const Body& body);
    bool updateBuffer(ID3D12ResourcePtr& dst, ID3D12ResourcePtr& staging, const void* src, size_t size);

    template<class Body>
    void writeTexture(ID3D12Resource* dst, ID3D12Resource* staging, UINT width, UINT height, DXGI_FORMAT format, const Body& src);
    void uploadTexture(ID3D12Resource* dst, ID3D12Resource* staging, const void* src, UINT width, UINT height, DXGI_FORMAT format);
    void uploadTexture(ID3D12Resource* dst, ID3D12Resource* staging, UINT width, UINT height, DXGI_FORMAT format);
    void copyTexture(ID3D12Resource* dst, ID3D12Resource* src, UINT width, UINT height, DXGI_FORMAT format);
    void readbackTexture(void* dst, ID3D12Resource* staging, UINT width, UINT height, DXGI_FORMAT format);
    void copyResource(ID3D12Resource* dst, ID3D12Resource* src);

    void createBufferSRV(DescriptorHandleDXR& handle, ID3D12Resource* res, size_t stride, size_t offset = 0);
    void createBufferUAV(DescriptorHandleDXR& handle, ID3D12Resource* res, size_t stride, size_t offset = 0);
    void createTextureSRV(DescriptorHandleDXR& handle, ID3D12Resource* res, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
    void createTextureUAV(DescriptorHandleDXR& handle, ID3D12Resource* res, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
    void createCBV(DescriptorHandleDXR& handle, ID3D12Resource* res, size_t size, size_t offset = 0);
    void createTextureRTV(DescriptorHandleDXR& handle, ID3D12Resource* res, DXGI_FORMAT format);

public:
    EntityList<CameraDXR>       m_cameras;
    EntityList<LightDXR>        m_lights;
    EntityList<RenderTargetDXR> m_render_targets;
    EntityList<TextureDXR>      m_textures;
    EntityList<MaterialDXR>     m_materials;
    EntityList<MeshDXR>         m_meshes;
    EntityList<MeshInstanceDXR> m_mesh_instances;
    EntityList<SceneDXR>        m_scenes;
    HandleAllocator m_vb_handles;
    HandleAllocator m_ib_handles;
    HandleAllocator m_deform_mesh_handles;
    HandleAllocator m_deform_instance_handles;


    std::string m_device_name;
    IDXGIFactory4Ptr m_dxgi_factory;
    ID3D12Device5Ptr m_device;
    ID3D12CommandQueuePtr m_cmd_queue_direct;
    ID3D12CommandQueuePtr m_cmd_queue_compute;
    CommandListManagerDXRPtr m_clm_direct;
    ID3D12GraphicsCommandList4Ptr m_cl;
    DeformerDXRPtr m_deformer;
    ID3D12FencePtr m_fence;
    uint64_t m_fence_value = 0;
    uint64_t m_fv_upload = 0;
    uint64_t m_fv_deform = 0;
    uint64_t m_fv_blas = 0;
    uint64_t m_fv_tlas = 0;
    uint64_t m_fv_rays = 0;
    FenceEventDXR m_fence_event;

    ID3D12RootSignaturePtr m_rootsig;
    ID3D12StateObjectPtr m_pipeline_state;
    uint64_t m_shader_record_size = 0;

    ID3D12ResourcePtr m_buf_shader_table, m_buf_shader_table_staging;
    ID3D12ResourcePtr m_buf_lights, m_buf_lights_staging;
    ID3D12ResourcePtr m_buf_materials, m_buf_materials_staging;
    ID3D12ResourcePtr m_buf_instances, m_buf_instances_staging;
    ID3D12ResourcePtr m_buf_meshes, m_buf_meshes_staging;

    ID3D12DescriptorHeapPtr m_desc_heap_srv;
    ID3D12DescriptorHeapPtr m_desc_heap_sampler;
    DescriptorHeapAllocatorDXR m_desc_alloc_srv;
    DescriptorHeapAllocatorDXR m_desc_alloc_sampler;

    DescriptorHandleDXR m_srv_instances;
    DescriptorHandleDXR m_srv_meshes;
    DescriptorHandleDXR m_srv_materials;
    DescriptorHandleDXR m_srv_indices;
    DescriptorHandleDXR m_srv_vertices;
    DescriptorHandleDXR m_srv_textures;
    DescriptorHandleDXR m_srv_deform_meshes;
    DescriptorHandleDXR m_srv_deform_instances;
    DescriptorHandleDXR m_sampler_default;

#ifdef gptEnableTimestamp
    TimestampDXRPtr m_timestamp;
#endif // gptEnableTimestamp
};
gptDefRefPtr(ContextDXR);
gptDefDXRT(ContextDXR, IContext)



template<class Body>
inline void ContextDXR::writeBuffer(ID3D12Resource* dst, ID3D12Resource* staging, UINT64 size, const Body& src)
{
    if (Map(staging, src)) {
        copyBuffer(dst, staging, size);
    }
}

template<class Body>
inline bool ContextDXR::updateBuffer(ID3D12ResourcePtr& dst, ID3D12ResourcePtr& staging, size_t size, const Body& body)
{
    size = std::max<size_t>(size, 256);
    bool allocated = false;
    if (GetSize(dst) < size) {
        dst = createBuffer(size);
        staging = createUploadBuffer(size);
        allocated = true;
    }
    writeBuffer(dst, staging, size, body);
    return allocated;
}

template<class Body>
inline void ContextDXR::writeTexture(ID3D12Resource* dst, ID3D12Resource* staging, UINT width, UINT height, DXGI_FORMAT format, const Body& src)
{
    if (Map(staging, src)) {
        uploadTexture(dst, staging, width, height, format);
    }
}

} // namespace gpt
#endif // _WIN32
