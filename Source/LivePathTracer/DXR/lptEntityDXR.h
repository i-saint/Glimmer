#pragma once
#ifdef _WIN32
#include "lptEntity.h"
#include "lptInternalDXR.h"

namespace lpt {


class ContextDXR;

template<class T>
class DXREntity : public T
{
using super = T;
public:
    template<class... T>
    DXREntity(T&&... v)
        : super(std::forward<T>(v)...)
    {
    }

public:
    ContextDXR* m_context = nullptr;
};


class RenderTargetDXR : public DXREntity<RenderTarget>
{
using super = DXREntity<RenderTarget>;
friend class ContextDXR;
public:
    RenderTargetDXR(TextureFormat format, int width, int height);
    bool readback(void* dst) override;
    void* getDeviceObject() override;

    void updateResources();

public:
    ID3D12ResourcePtr m_frame_buffer;
    ID3D12ResourcePtr m_accum_buffer;
    ID3D12ResourcePtr m_buf_readback;
};
lptDeclRefPtr(RenderTargetDXR);


class TextureDXR : public DXREntity<Texture>
{
using super = DXREntity<Texture>;
friend class ContextDXR;
public:
    TextureDXR(TextureFormat format, int width, int height);
    void* getDeviceObject() override;

    void updateResources();

public:
    ID3D12ResourcePtr m_texture;
    ID3D12ResourcePtr m_buf_upload;
    DescriptorHandleDXR m_srv;
};
lptDeclRefPtr(TextureDXR);


class MaterialDXR : public DXREntity<Material>
{
using super = DXREntity<Material>;
friend class ContextDXR;
public:

public:
};
lptDeclRefPtr(MaterialDXR);


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


class MeshDXR : public DXREntity<Mesh>
{
using super = DXREntity<Mesh>;
friend class ContextDXR;
public:
    void updateResources();
    void updateBLAS();
    void clearBLAS();

public:
    // vertex buffers
    ID3D12ResourcePtr m_buf_indices, m_buf_indices_staging;
    ID3D12ResourcePtr m_buf_vertices, m_buf_vertices_staging;
    ID3D12ResourcePtr m_buf_faces, m_buf_faces_staging;
    DescriptorHandleDXR m_srv_vertices;
    DescriptorHandleDXR m_srv_faces;

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
    void updateResources();
    void updateBLAS();
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
    void updateResources();
    void updateTLAS();

public:
    ID3D12GraphicsCommandList4Ptr m_cl_deform;
    DescriptorHandleDXR m_uav_frame_buffer;
    DescriptorHandleDXR m_uav_accum_buffer;
    DescriptorHandleDXR m_srv_tlas;
    DescriptorHandleDXR m_srv_prev_buffer;
    DescriptorHandleDXR m_cbv_scene;
    DescriptorHandleDXR m_srv_tmp;

    ID3D12ResourcePtr m_tlas_instance_desc;
    ID3D12ResourcePtr m_tlas_scratch;
    ID3D12ResourcePtr m_tlas;
    ID3D12ResourcePtr m_buf_scene, m_buf_scene_staging;

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
