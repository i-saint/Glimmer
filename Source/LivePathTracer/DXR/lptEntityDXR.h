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

#define lptDefDXRT(T, I)\
    inline T* dxr_t(I* v) { return static_cast<T*>(v); }\
    inline T& dxr_t(I& v) { return static_cast<T&>(v); }


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
lptDefRefPtr(RenderTargetDXR);
lptDefDXRT(RenderTargetDXR, IRenderTarget)


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
lptDefRefPtr(TextureDXR);
lptDefDXRT(TextureDXR, ITexture)


class MaterialDXR : public DXREntity<Material>
{
using super = DXREntity<Material>;
friend class ContextDXR;
public:
public:
};
lptDefRefPtr(MaterialDXR);
lptDefDXRT(MaterialDXR, IMaterial)


class CameraDXR : public DXREntity<Camera>
{
using super = DXREntity<Camera>;
friend class ContextDXR;
public:
public:
};
lptDefRefPtr(CameraDXR);
lptDefDXRT(CameraDXR, ICamera)


class LightDXR : public DXREntity<Light>
{
using super = DXREntity<Light>;
friend class ContextDXR;
public:

public:
};
lptDefRefPtr(LightDXR);
lptDefDXRT(LightDXR, ILight)


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
    ID3D12ResourcePtr m_buf_bs, m_buf_bs_staging;
    ID3D12ResourcePtr m_buf_bs_frames, m_buf_bs_frames_staging;
    ID3D12ResourcePtr m_buf_bs_delta, m_buf_bs_delta_staging;

    // skinning data
    ID3D12ResourcePtr m_buf_joint_counts, m_buf_joint_counts_staging;
    ID3D12ResourcePtr m_buf_joint_weights, m_buf_joint_weights_staging;

    // acceleration structure
    ID3D12ResourcePtr m_blas;
    ID3D12ResourcePtr m_blas_scratch;
    bool m_blas_updated = false;
};
lptDefRefPtr(MeshDXR);
lptDefDXRT(MeshDXR, IMesh)


class MeshInstanceDXR : public DXREntity<MeshInstance>
{
using super = DXREntity<MeshInstance>;
friend class ContextDXR;
public:
    MeshInstanceDXR(IMesh* v = nullptr);
    ~MeshInstanceDXR();
    void updateResources();
    void updateBLAS();
    void clearBLAS();

public:
    // deformation
    ID3D12ResourcePtr m_buf_vertices;
    ID3D12ResourcePtr m_buf_bs_weights;
    ID3D12ResourcePtr m_buf_joint_matrices;
    DescriptorHandleDXR m_uav_vertices;
    DescriptorHandleDXR m_srv_bs_weights;
    DescriptorHandleDXR m_srv_joint_matrices;

    // acceleration structure
    ID3D12ResourcePtr m_blas;
    ID3D12ResourcePtr m_blas_scratch;
    bool m_blas_updated = false;
};
lptDefRefPtr(MeshInstanceDXR);
lptDefDXRT(MeshInstanceDXR, IMeshInstance)


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
lptDefRefPtr(SceneDXR);
lptDefDXRT(SceneDXR, IScene)

} // namespace lpt
#endif // _WIN32
