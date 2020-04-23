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


class CameraDXR : public Camera
{
using super = Camera;
friend class ContextDXR;
public:

public:
};
lptDeclRefPtr(CameraDXR);


class LightDXR : public Light
{
using super = Light;
friend class ContextDXR;
public:

public:
};
lptDeclRefPtr(LightDXR);


class TextureDXR : public Texture
{
using super = Texture;
friend class ContextDXR;
public:
    bool upload(const void* src) override;
    void* getDeviceObject() override;

public:
    ID3D12ResourcePtr m_texture;
    DescriptorHandleDXR m_uav;
};
lptDeclRefPtr(TextureDXR);


class RenderTargetDXR : public RenderTarget
{
using super = RenderTarget;
friend class ContextDXR;
public:
    bool readback(void* dst) override;
    void* getDeviceObject() override;

public:
    ID3D12ResourcePtr m_texture;
    DescriptorHandleDXR m_uav;
};
lptDeclRefPtr(RenderTargetDXR);


class MaterialDXR : public Material
{
using super = Material;
friend class ContextDXR;
public:

public:
};
lptDeclRefPtr(MaterialDXR);


class MeshDXR : public Mesh
{
using super = Mesh;
friend class ContextDXR;
public:
    void clearBLAS();

public:
    ID3D12ResourcePtr m_buf_index;
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

    ID3D12ResourcePtr m_blas; // bottom level acceleration structure
    ID3D12ResourcePtr m_blas_scratch;

};
lptDeclRefPtr(MeshDXR);


class MeshInstanceDXR : public MeshInstance
{
using super = MeshInstance;
friend class ContextDXR;
public:
    void clearBLAS();

public:
    MeshDXRPtr m_mesh;
    ID3D12DescriptorHeapPtr m_desc_heap;
    ID3D12ResourcePtr m_buf_bs_weights;
    ID3D12ResourcePtr m_buf_joint_matrices;
    ID3D12ResourcePtr m_buf_points_deformed;
    ID3D12ResourcePtr m_blas_deformed;
    ID3D12ResourcePtr m_blas_scratch;
    bool m_is_updated = false;

};
lptDeclRefPtr(MeshInstanceDXR);


class SceneDXR : public Scene
{
using super = Scene;
friend class ContextDXR;
public:
    bool hasFlag(RenderFlag f) const;
    void clear();

public:
    ID3D12GraphicsCommandList4Ptr cl_deform;
    ID3D12DescriptorHeapPtr desc_heap;
    DescriptorHandleDXR render_target_uav;
    DescriptorHandleDXR vertex_buffer_srv;
    DescriptorHandleDXR material_data_srv;
    DescriptorHandleDXR instance_data_srv;
    DescriptorHandleDXR scene_data_cbv;
    DescriptorHandleDXR back_buffer_uav, back_buffer_srv;

    std::vector<MeshInstanceDXRPtr> instances, instances_prev;
    SceneData scene_data_prev{};
    TLASDataDXR tlas_data;
    ID3D12ResourcePtr instance_data;
    ID3D12ResourcePtr scene_data;
    RenderTargetDXRPtr render_target;

    uint64_t fv_translate = 0, fv_deform = 0, fv_blas = 0, fv_tlas = 0, fv_rays = 0;
    FenceEventDXR fence_event;
    uint32_t render_flags = 0;

#ifdef lptEnableTimestamp
    TimestampDXRPtr timestamp;
#endif // lptEnableTimestamp
};
lptDeclRefPtr(SceneDXR);



class ContextDXR : public Context
{
using super = Context;
public:
    CameraDXR*       createCamera() override;
    LightDXR*        createLight() override;
    RenderTargetDXR* createRenderTarget() override;
    TextureDXR*      createTexture() override;
    MaterialDXR*     createMaterial() override;
    MeshDXR*         createMesh() override;
    MeshInstanceDXR* createMeshInstance() override;
    SceneDXR*        createScene() override;

    void renderStart(IScene* v) override;
    void renderFinish(IScene* v) override;

public:
    ID3D12ResourcePtr vertex_buffer;
    ID3D12ResourcePtr material_data;
};
lptDeclRefPtr(ContextDXR);


} // namespace lpt
#endif // _WIN32
