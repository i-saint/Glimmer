#pragma once
#ifdef _WIN32
#include "lptEntityDXR.h"
#include "Foundation/lptUtils.h"

namespace lpt {

#define lptDXRMaxTraceRecursionLevel  2
#define lptDXRMaxDescriptorCount 8192
#define lptDXRMaxMeshCount 2048
#define lptDXRMaxTextureCount 2048
#define lptDXRMaxShaderRecords 64
#define lptSwapChainBuffers 2


class ContextDXR : public DXREntity<Context>
{
using super = DXREntity<Context>;
public:
    ContextDXR();
    ~ContextDXR();

    ICameraPtr       createCamera() override;
    ILightPtr        createLight() override;
    IRenderTargetPtr createRenderTarget(TextureFormat format, int width, int height) override;
    ITexturePtr      createTexture(TextureFormat format, int width, int height) override;
    IMaterialPtr     createMaterial() override;
    IMeshPtr         createMesh() override;
    IMeshInstancePtr createMeshInstance(IMesh* v) override;
    IScenePtr        createScene() override;

    void render() override;
    void finish() override;
    void* getDevice() override;
    const char* getTimestampLog() override;

    void clear();
    bool checkError();
    bool initializeDevice();
    void prepare();
    void updateResources();
    void deform();
    void updateBLAS();
    void updateTLAS();
    void dispatchRays();

    uint64_t incrementFenceValue();

    ID3D12ResourcePtr createBuffer(uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, const D3D12_HEAP_PROPERTIES& heap_props);
    ID3D12ResourcePtr createBuffer(uint64_t size);
    ID3D12ResourcePtr createUploadBuffer(uint64_t size);

    ID3D12ResourcePtr createTexture(int width, int height, DXGI_FORMAT format);
    ID3D12ResourcePtr createTextureUploadBuffer(int width, int height, DXGI_FORMAT format);
    ID3D12ResourcePtr createTextureReadbackBuffer(int width, int height, DXGI_FORMAT format);


    uint64_t submit(uint64_t preceding_fv = 0);
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

    void createBufferSRV(DescriptorHandleDXR& handle, ID3D12Resource* res, size_t stride);
    void createTextureSRV(DescriptorHandleDXR& handle, ID3D12Resource* res);
    void createTextureUAV(DescriptorHandleDXR& handle, ID3D12Resource* res);
    void createCBV(DescriptorHandleDXR& handle, ID3D12Resource* res, size_t size);
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


    IDXGIFactory4Ptr m_dxgi_factory;
    ID3D12Device5Ptr m_device;
    ID3D12CommandQueuePtr m_cmd_queue_direct;
    CommandListManagerDXRPtr m_clm_direct;
    ID3D12GraphicsCommandList4Ptr m_cl;
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
    ID3D12ResourcePtr m_buf_instances, m_buf_instances_staging;
    ID3D12ResourcePtr m_buf_materials, m_buf_materials_staging;
    ID3D12ResourcePtr m_buf_meshes, m_buf_meshes_staging;

    ID3D12DescriptorHeapPtr m_desc_heap;
    DescriptorHeapAllocatorDXR m_desc_alloc;
    DescriptorHandleDXR m_srv_instances;
    DescriptorHandleDXR m_srv_meshes;
    DescriptorHandleDXR m_srv_materials;
    DescriptorHandleDXR m_srv_vertices;
    DescriptorHandleDXR m_srv_faces;
    DescriptorHandleDXR m_srv_vertices_deformed;
    DescriptorHandleDXR m_srv_textures;

#ifdef lptEnableTimestamp
    TimestampDXRPtr m_timestamp;
#endif // lptEnableTimestamp
};
lptDefRefPtr(ContextDXR);
lptDefDXRT(ContextDXR, IContext)



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

} // namespace lpt
#endif // _WIN32
