#pragma once
#ifdef _WIN32
#include "lptEntity.h"
#include "lptInternalDXR.h"

namespace lpt {

struct InstanceDataDXR
{
    uint32_t mesh_index;
    uint32_t material_index;
    uint32_t instance_flags; // combination of InstanceFlags
    uint32_t layer_mask;
};

class SceneDataDXR : public SceneData
{
};


class ContextDXR;

template<class T>
class DXREntity : public T
{
public:

public:
    ContextDXR* m_context = nullptr;
};


class CameraDXR : public DXREntity<Camera>
{
using super = Camera;
friend class ContextDXR;
public:

public:
};
lptDeclRefPtr(CameraDXR);


class LightDXR : public DXREntity<Light>
{
using super = Light;
friend class ContextDXR;
public:

public:
};
lptDeclRefPtr(LightDXR);


class TextureDXR : public DXREntity<Texture>
{
using super = Texture;
friend class ContextDXR;
public:
    void upload(const void* src) override;
    void* getDeviceObject() override;

public:
    ID3D12ResourcePtr m_texture;
    DescriptorHandleDXR m_uav;
};
lptDeclRefPtr(TextureDXR);


class RenderTargetDXR : public DXREntity<RenderTarget>
{
using super = RenderTarget;
friend class ContextDXR;
public:
    void readback(void* dst) override;
    void* getDeviceObject() override;

public:
    ID3D12ResourcePtr m_texture;
    DescriptorHandleDXR m_uav;
};
lptDeclRefPtr(RenderTargetDXR);


class MaterialDXR : public DXREntity<Material>
{
using super = Material;
friend class ContextDXR;
public:

public:
};
lptDeclRefPtr(MaterialDXR);


class MeshDXR : public DXREntity<Mesh>
{
using super = Mesh;
friend class ContextDXR;
public:
    void clearBLAS();

public:
    // vertex buffers
    ID3D12ResourcePtr m_buf_indices;
    ID3D12ResourcePtr m_buf_points;
    ID3D12ResourcePtr m_buf_normals;
    ID3D12ResourcePtr m_buf_tangents;
    ID3D12ResourcePtr m_buf_uv;
    ID3D12ResourcePtr m_buf_mesh_info;

    // blendshape data
    ID3D12ResourcePtr m_buf_bs_delta;
    ID3D12ResourcePtr m_buf_bs_frames;
    ID3D12ResourcePtr m_buf_bs_info;

    // skinning data
    ID3D12ResourcePtr m_buf_joint_counts;
    ID3D12ResourcePtr m_buf_joint_weights;

    // acceleration structure
    ID3D12ResourcePtr m_blas;
    ID3D12ResourcePtr m_blas_scratch;
    bool m_blas_updated = false;

};
lptDeclRefPtr(MeshDXR);


class MeshInstanceDXR : public DXREntity<MeshInstance>
{
using super = MeshInstance;
friend class ContextDXR;
public:
    void clearBLAS();

public:
    MeshDXRPtr m_mesh;
    ID3D12DescriptorHeapPtr m_desc_heap;

    // deformation
    ID3D12ResourcePtr m_buf_bs_weights;
    ID3D12ResourcePtr m_buf_joint_matrices;
    ID3D12ResourcePtr m_buf_points_deformed;

    // acceleration structure
    ID3D12ResourcePtr m_blas_deformed;
    ID3D12ResourcePtr m_blas_scratch;
    bool m_blas_updated = false;
};
lptDeclRefPtr(MeshInstanceDXR);


class SceneDXR : public DXREntity<Scene>
{
using super = Scene;
friend class ContextDXR;
public:
    bool hasFlag(RenderFlag f) const;
    void clear();

public:
    ID3D12GraphicsCommandList4Ptr m_cl_deform;
    ID3D12DescriptorHeapPtr m_desc_heap;
    DescriptorHandleDXR m_render_target_uav;
    DescriptorHandleDXR m_instance_data_srv;
    DescriptorHandleDXR m_scene_data_cbv;

    std::vector<MeshInstanceDXRPtr> m_instances;
    std::vector<MeshInstanceDXRPtr> m_instances_prev;
    SceneData m_scene_data_prev{};
    TLASDataDXR m_tlas_data;
    ID3D12ResourcePtr m_instance_data;
    ID3D12ResourcePtr m_scene_data;
    RenderTargetDXRPtr m_render_target;

    uint64_t m_fv_tlas = 0;
    uint64_t m_fv_rays = 0;
    FenceEventDXR m_fence_event;
    uint32_t m_render_flags = 0;

#ifdef lptEnableTimestamp
    TimestampDXRPtr m_timestamp;
#endif // lptEnableTimestamp
};
lptDeclRefPtr(SceneDXR);



class ContextDXR : public DXREntity<Context>
{
using super = Context;
public:
    ContextDXR();
    ~ContextDXR();

    CameraDXR*       createCamera() override;
    LightDXR*        createLight() override;
    RenderTargetDXR* createRenderTarget() override;
    TextureDXR*      createTexture() override;
    MaterialDXR*     createMaterial() override;
    MeshDXR*         createMesh() override;
    MeshInstanceDXR* createMeshInstance() override;
    SceneDXR*        createScene() override;

    void frameBegin() override;
    void renderBegin(IScene* v) override;
    void renderEnd(IScene* v) override;
    void frameEnd() override;
    void* getDevice() override;

    void setupMeshes();
    void setupScene(SceneDXR& scene);


    bool initializeDevice();
    uint64_t incrementFenceValue();
    ID3D12ResourcePtr createBuffer(uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, const D3D12_HEAP_PROPERTIES& heap_props);
    ID3D12ResourcePtr createTexture(int width, int height, DXGI_FORMAT format);

    void addResourceBarrier(ID3D12GraphicsCommandList* cl, ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after);
    uint64_t submitResourceBarrier(ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after, uint64_t preceding_fv = 0);
    uint64_t submitDirectCommandList(ID3D12GraphicsCommandList* cl, uint64_t preceding_fv = 0);
    uint64_t submitComputeCommandList(ID3D12GraphicsCommandList* cl, uint64_t preceding_fv = 0);
    uint64_t submitCommandList(ID3D12CommandQueue* cq, ID3D12GraphicsCommandList* cl, uint64_t preceding_fv = 0);

    uint64_t readbackBuffer(void* dst, ID3D12Resource* src, UINT64 size);
    uint64_t uploadBuffer(ID3D12Resource* dst, const void* src, UINT64 size, bool immediate = true);
    uint64_t copyBuffer(ID3D12Resource* dst, ID3D12Resource* src, UINT64 size, bool immediate = true);
    uint64_t readbackTexture(void* dst, ID3D12Resource* src, UINT width, UINT height, DXGI_FORMAT format);
    uint64_t uploadTexture(ID3D12Resource* dst, const void* src, UINT width, UINT height, DXGI_FORMAT format, bool immediate = true);
    uint64_t copyTexture(ID3D12Resource* dst, ID3D12Resource* src, bool immediate = true, uint64_t preceding_fv = 0);
    uint64_t submitCopy(ID3D12GraphicsCommandList4Ptr& cl, bool immediate, uint64_t preceding_fv = 0);

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
    bool m_power_stable_state = false;

    ID3D12CommandQueuePtr m_cmd_queue_direct;
    ID3D12CommandQueuePtr m_cmd_queue_compute;
    ID3D12CommandQueuePtr m_cmd_queue_copy;
    ID3D12FencePtr m_fence;
    uint64_t m_fence_value = 0;
    uint64_t m_fv_upload = 0;
    uint64_t m_fv_deform = 0;
    uint64_t m_fv_blas = 0;
    uint64_t m_fv_last_rays = 0;

    CommandListManagerDXRPtr m_clm_direct;
    CommandListManagerDXRPtr m_clm_copy;
    FenceEventDXR m_event_copy;
    std::vector<ID3D12ResourcePtr> m_tmp_resources;

    ID3D12RootSignaturePtr m_rootsig;
    ID3D12StateObjectPtr m_pipeline_state;
    ID3D12ResourcePtr m_shader_table;
    uint64_t m_shader_record_size = 0;

    ID3D12ResourcePtr m_vertex_buffer;
    ID3D12ResourcePtr m_material_data;

#ifdef lptEnableTimestamp
    TimestampDXRPtr m_timestamp;
#endif // lptEnableTimestamp
};
lptDeclRefPtr(ContextDXR);


} // namespace lpt
#endif // _WIN32
