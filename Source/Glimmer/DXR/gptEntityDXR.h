#pragma once
#ifdef _WIN32
#include "gptEntity.h"
#include "gptInternalDXR.h"

namespace gpt {

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

#define gptDefDXRT(T, I)\
    inline T* dxr_t(I* v) { return static_cast<T*>(v); }\
    inline T& dxr_t(I& v) { return static_cast<T&>(v); }


class RenderTargetDXR : public DXREntity<RenderTarget>
{
using super = DXREntity<RenderTarget>;
friend class ContextDXR;
public:
    RenderTargetDXR(int width, int height, Format format);
    RenderTargetDXR(IWindow* window, Format format);
    bool readback(void* dst) override;
    void* getDeviceObject() const override;

    void updateResources();
    void readback();
    void present();

public:
    class WindowCallback : public IWindowCallback
    {
    public:
        void onResize(int w, int h) override;

        RenderTargetDXR* m_self = nullptr;
    };

    ID3D12ResourcePtr m_frame_buffer;
    ID3D12ResourcePtr m_accum_buffer;
    ID3D12ResourcePtr m_buf_readback;

    DescriptorHandleDXR m_uav_frame_buffer;
    DescriptorHandleDXR m_uav_accum_buffer;

    WindowCallback m_callback;
    SwapchainDXRPtr m_swapchain;
};
gptDefRefPtr(RenderTargetDXR);
gptDefDXRT(RenderTargetDXR, IRenderTarget)


class TextureDXR : public DXREntity<Texture>
{
using super = DXREntity<Texture>;
friend class ContextDXR;
public:
    TextureDXR(int width, int height, Format format);
    void* getDeviceObject() const override;

    void updateResources();

public:
    ID3D12ResourcePtr m_texture;
    ID3D12ResourcePtr m_buf_upload;
    DescriptorHandleDXR m_srv;
};
gptDefRefPtr(TextureDXR);
gptDefDXRT(TextureDXR, ITexture)


class MaterialDXR : public DXREntity<Material>
{
using super = DXREntity<Material>;
friend class ContextDXR;
public:
public:
};
gptDefRefPtr(MaterialDXR);
gptDefDXRT(MaterialDXR, IMaterial)


class CameraDXR : public DXREntity<Camera>
{
using super = DXREntity<Camera>;
friend class ContextDXR;
public:
public:
};
gptDefRefPtr(CameraDXR);
gptDefDXRT(CameraDXR, ICamera)


class LightDXR : public DXREntity<Light>
{
using super = DXREntity<Light>;
friend class ContextDXR;
public:

public:
};
gptDefRefPtr(LightDXR);
gptDefDXRT(LightDXR, ILight)


class MeshDXR : public DXREntity<Mesh>
{
using super = DXREntity<Mesh>;
friend class ContextDXR;
public:
    MeshDXR();
    ~MeshDXR();
    void prepare();
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

    // deform buffers
    ID3D12ResourcePtr m_buf_joint_counts, m_buf_joint_counts_staging;
    ID3D12ResourcePtr m_buf_joint_weights, m_buf_joint_weights_staging;
    ID3D12ResourcePtr m_buf_bs, m_buf_bs_staging;
    ID3D12ResourcePtr m_buf_bs_frames, m_buf_bs_frames_staging;
    ID3D12ResourcePtr m_buf_bs_delta, m_buf_bs_delta_staging;
    DescriptorHandleDXR m_srv_joint_counts;
    DescriptorHandleDXR m_srv_joint_weights;
    DescriptorHandleDXR m_srv_bs;
    DescriptorHandleDXR m_srv_bs_frames;
    DescriptorHandleDXR m_srv_bs_delta;
    DescriptorHandleDXR m_srv_mesh;

    // acceleration structure
    ID3D12ResourcePtr m_blas;
    ID3D12ResourcePtr m_blas_scratch;
    bool m_blas_updated = false;
};
gptDefRefPtr(MeshDXR);
gptDefDXRT(MeshDXR, IMesh)


class MeshInstanceDXR : public DXREntity<MeshInstance>
{
using super = DXREntity<MeshInstance>;
friend class ContextDXR;
public:
    MeshInstanceDXR(IMesh* v = nullptr);
    ~MeshInstanceDXR();
    void prepare();
    void updateResources();
    void updateBLAS();
    void clearBLAS();

public:
    // deformation
    ID3D12ResourcePtr m_buf_vertices;
    ID3D12ResourcePtr m_buf_bs_weights, m_buf_bs_weights_staging;
    ID3D12ResourcePtr m_buf_joint_matrices, m_buf_joint_matrices_staging;
    DescriptorHandleDXR m_srv_vertices;
    DescriptorHandleDXR m_uav_vertices;
    DescriptorHandleDXR m_srv_joint_matrices;
    DescriptorHandleDXR m_srv_bs_weights;

    // acceleration structure
    ID3D12ResourcePtr m_blas;
    ID3D12ResourcePtr m_blas_scratch;
    bool m_blas_updated = false;
};
gptDefRefPtr(MeshInstanceDXR);
gptDefDXRT(MeshInstanceDXR, IMeshInstance)


class SceneDXR : public DXREntity<Scene>
{
using super = DXREntity<Scene>;
friend class ContextDXR;
public:
    void prepare();
    void updateResources();
    void updateTLAS();

public:
    ID3D12GraphicsCommandList4Ptr m_cl_deform;

    ID3D12ResourcePtr m_tlas_instance_desc;
    ID3D12ResourcePtr m_tlas_scratch;
    ID3D12ResourcePtr m_tlas;
    ID3D12ResourcePtr m_buf_lights, m_buf_lights_staging;
    ID3D12ResourcePtr m_buf_meshlights, m_buf_meshlights_staging;
    ID3D12ResourcePtr m_buf_scene, m_buf_scene_staging;
    ID3D12ResourcePtr m_buf_rfaces, m_buf_rfaces_staging;

    DescriptorHandleDXR m_srv_tlas;
    DescriptorHandleDXR m_srv_lights;
    DescriptorHandleDXR m_srv_meshlights;
    DescriptorHandleDXR m_cbv_scene;
    DescriptorHandleDXR m_srv_rfaces;

    uint32_t m_render_flags = 0;

    RawVector<face_t> m_refractive_faces;
};
gptDefRefPtr(SceneDXR);
gptDefDXRT(SceneDXR, IScene)

} // namespace gpt
#endif // _WIN32
