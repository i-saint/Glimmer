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
using super = DXREntity<Camera>;
friend class ContextDXR;
public:

public:
};
lptDeclRefPtr(CameraDXR);


class LightDXR : public DXREntity<Light>
{
using super = DXREntity<Light>;
friend class ContextDXR;
public:

public:
};
lptDeclRefPtr(LightDXR);


class TextureDXR : public DXREntity<Texture>
{
using super = DXREntity<Texture>;
friend class ContextDXR;
public:
    void* getDeviceObject() override;

public:
    ID3D12ResourcePtr m_texture;
    ID3D12ResourcePtr m_upload_buffer;
    DescriptorHandleDXR m_uav;
};
lptDeclRefPtr(TextureDXR);


class RenderTargetDXR : public DXREntity<RenderTarget>
{
using super = DXREntity<RenderTarget>;
friend class ContextDXR;
public:
    void* getDeviceObject() override;

public:
    ID3D12ResourcePtr m_texture;
    ID3D12ResourcePtr m_readback_buffer;
    DescriptorHandleDXR m_uav;
};
lptDeclRefPtr(RenderTargetDXR);


class MaterialDXR : public DXREntity<Material>
{
using super = DXREntity<Material>;
friend class ContextDXR;
public:

public:
};
lptDeclRefPtr(MaterialDXR);


class MeshDXR : public DXREntity<Mesh>
{
using super = DXREntity<Mesh>;
friend class ContextDXR;
public:
    void clearBLAS();

public:
    // vertex buffers
    ID3D12ResourcePtr m_buf_indices, m_buf_indices_staging;
    ID3D12ResourcePtr m_buf_points, m_buf_points_staging;
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
using super = DXREntity<MeshInstance>;
friend class ContextDXR;
public:
    MeshInstanceDXR(IMesh* v = nullptr);
    void clearBLAS();

public:
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
using super = DXREntity<Scene>;
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

    SceneData m_scene_data_prev{};
    TLASDataDXR m_tlas_data;
    ID3D12ResourcePtr m_instance_data;
    ID3D12ResourcePtr m_scene_data;
    RenderTargetDXRPtr m_render_target;

    uint32_t m_render_flags = 0;
};
lptDeclRefPtr(SceneDXR);



class ContextDXR : public DXREntity<Context>
{
using super = DXREntity<Context>;
public:
    ContextDXR();
    ~ContextDXR();

    CameraDXR*       createCamera() override;
    LightDXR*        createLight() override;
    RenderTargetDXR* createRenderTarget() override;
    TextureDXR*      createTexture() override;
    MaterialDXR*     createMaterial() override;
    MeshDXR*         createMesh() override;
    MeshInstanceDXR* createMeshInstance(IMesh* v) override;
    SceneDXR*        createScene() override;

    void render() override;
    void finish() override;
    void* getDevice() override;

    void updateEntities();
    void setupMaterials();
    void setupMeshes();
    void setupScenes();
    void dispatchRays();
    void resetState();


    bool checkError();
    bool initializeDevice();
    uint64_t incrementFenceValue();

    ID3D12ResourcePtr createBuffer(uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, const D3D12_HEAP_PROPERTIES& heap_props);
    ID3D12ResourcePtr createBuffer(uint64_t size);
    ID3D12ResourcePtr createUploadBuffer(uint64_t size);

    ID3D12ResourcePtr createTexture(int width, int height, DXGI_FORMAT format);
    ID3D12ResourcePtr createTextureUploadBuffer(int width, int height, DXGI_FORMAT format);
    ID3D12ResourcePtr createTextureReadbackBuffer(int width, int height, DXGI_FORMAT format);


    uint64_t insertSignal();
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
    uint64_t m_fv_readback = 0;
    FenceEventDXR m_fence_event;

    FenceEventDXR m_event_copy;
    std::vector<ID3D12ResourcePtr> m_tmp_resources;

    ID3D12RootSignaturePtr m_rootsig;
    ID3D12StateObjectPtr m_pipeline_state;
    ID3D12ResourcePtr m_shader_table;
    uint64_t m_shader_record_size = 0;

    ID3D12ResourcePtr m_buf_vertices;
    ID3D12ResourcePtr m_buf_materials, m_buf_materials_staging;

#ifdef lptEnableTimestamp
    TimestampDXRPtr m_timestamp;
#endif // lptEnableTimestamp
};
lptDeclRefPtr(ContextDXR);


#define DefDXRT(T, I)\
    inline T* dxr_t(I* v) { return static_cast<T*>(v); }\
    inline T& dxr_t(I& v) { return static_cast<T&>(v); }

DefDXRT(CameraDXR, ICamera)
DefDXRT(LightDXR, ILight)
DefDXRT(TextureDXR, ITexture)
DefDXRT(RenderTargetDXR, IRenderTarget)
DefDXRT(MaterialDXR, IMaterial)
DefDXRT(MeshDXR, IMesh)
DefDXRT(MeshInstanceDXR, IMeshInstance)
DefDXRT(SceneDXR, IScene)
DefDXRT(ContextDXR, IContext)

#undef DefDXRT


} // namespace lpt
#endif // _WIN32
