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
    ID3D12ResourcePtr m_buf_upload;
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
    ID3D12ResourcePtr m_buf_readback;
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
    ID3D12ResourcePtr m_buf_instance_data;
    ID3D12ResourcePtr m_buf_scene_data;
    RenderTargetDXRPtr m_render_target;

    uint32_t m_render_flags = 0;
};
lptDeclRefPtr(SceneDXR);




#define lptDefDXRT(T, I)\
    inline T* dxr_t(I* v) { return static_cast<T*>(v); }\
    inline T& dxr_t(I& v) { return static_cast<T&>(v); }

lptDefDXRT(CameraDXR, ICamera)
lptDefDXRT(LightDXR, ILight)
lptDefDXRT(TextureDXR, ITexture)
lptDefDXRT(RenderTargetDXR, IRenderTarget)
lptDefDXRT(MaterialDXR, IMaterial)
lptDefDXRT(MeshDXR, IMesh)
lptDefDXRT(MeshInstanceDXR, IMeshInstance)
lptDefDXRT(SceneDXR, IScene)

} // namespace lpt
#endif // _WIN32
