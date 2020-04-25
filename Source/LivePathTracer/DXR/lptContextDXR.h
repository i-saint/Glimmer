#pragma once
#ifdef _WIN32
#include "lptEntityDXR.h"

namespace lpt {

class ContextDXR : public DXREntity<Context>
{
using super = DXREntity<Context>;
public:
    ContextDXR();
    ~ContextDXR();

    CameraDXR*       createCamera() override;
    LightDXR*        createLight() override;
    RenderTargetDXR* createRenderTarget(TextureFormat format, int width, int height) override;
    TextureDXR*      createTexture(TextureFormat format, int width, int height) override;
    MaterialDXR*     createMaterial() override;
    MeshDXR*         createMesh() override;
    MeshInstanceDXR* createMeshInstance(IMesh* v) override;
    SceneDXR*        createScene() override;

    void render() override;
    void finish() override;
    void* getDevice() override;
    const char* getTimestampLog() override;


    bool checkError();
    bool initializeDevice();
    void updateEntities();
    void updateBuffers();
    void deform();
    void updateBLAS();
    void updateTLAS();
    void dispatchRays();
    void resetState();

    uint64_t incrementFenceValue();

    ID3D12ResourcePtr createBuffer(uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, const D3D12_HEAP_PROPERTIES& heap_props);
    ID3D12ResourcePtr createBuffer(uint64_t size);
    ID3D12ResourcePtr createUploadBuffer(uint64_t size);

    ID3D12ResourcePtr createTexture(int width, int height, DXGI_FORMAT format);
    ID3D12ResourcePtr createTextureUploadBuffer(int width, int height, DXGI_FORMAT format);
    ID3D12ResourcePtr createTextureReadbackBuffer(int width, int height, DXGI_FORMAT format);


    uint64_t submit(uint64_t preceding_fv = 0);
    void addResourceBarrier(ID3D12GraphicsCommandList* cl, ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after);

    void uploadBuffer(ID3D12Resource* dst, ID3D12Resource* staging, const void* src, UINT64 size);
    void writeBuffer(ID3D12Resource* dst, ID3D12Resource* staging, UINT64 size, const std::function<void(void*)>& src);
    void copyBuffer(ID3D12Resource* dst, ID3D12Resource* src, UINT64 size);
    void readbackBuffer(void* dst, ID3D12Resource* staging, UINT64 size);

    void uploadTexture(ID3D12Resource* dst, ID3D12Resource* staging, const void* src, UINT width, UINT height, DXGI_FORMAT format);
    void copyTexture(ID3D12Resource* dst, ID3D12Resource* src, UINT width, UINT height, DXGI_FORMAT format);
    void readbackTexture(void* dst, ID3D12Resource* staging, UINT width, UINT height, DXGI_FORMAT format);

public:
    std::vector<CameraDXRPtr> m_cameras;
    std::vector<LightDXRPtr> m_lights;
    std::vector<RenderTargetDXRPtr> m_render_targets;
    std::vector<TextureDXRPtr> m_textures;
    std::vector<MaterialDXRPtr> m_materials;
    std::vector<MeshDXRPtr> m_meshes;
    std::vector<MeshInstanceDXRPtr> m_mesh_instances;
    std::vector<SceneDXRPtr> m_scenes;


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
    ID3D12ResourcePtr m_shader_table;
    uint64_t m_shader_record_size = 0;

    ID3D12ResourcePtr m_buf_instances;
    ID3D12ResourcePtr m_buf_materials;
    ID3D12ResourcePtr m_buf_meshes;
    ID3D12ResourcePtr m_buf_vertices, m_buf_vertices_staging;
    ID3D12ResourcePtr m_buf_faces, m_buf_faces_staging;
    ID3D12DescriptorHeapPtr m_desc_heap;
    DescriptorHandleDXR m_srv_instances;
    DescriptorHandleDXR m_srv_materials;
    DescriptorHandleDXR m_srv_meshes;
    DescriptorHandleDXR m_srv_vertices;
    DescriptorHandleDXR m_srv_faces;

#ifdef lptEnableTimestamp
    TimestampDXRPtr m_timestamp;
#endif // lptEnableTimestamp
};
lptDeclRefPtr(ContextDXR);

lptDefDXRT(ContextDXR, IContext)


} // namespace lpt
#endif // _WIN32
