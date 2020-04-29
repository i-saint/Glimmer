#pragma once
#include "MeshUtils/MeshUtils.h"
#include "Foundation/lptUtils.h"

#define lptDefRefPtr(T) using T##Ptr = ref_ptr<T, InternalReleaser<T>>
#define lptMaxLights 32

namespace lpt {

enum class GlobalFlag : uint32_t
{
    StrictUpdateCheck   = 0x00000001,
    PowerStableState    = 0x00000002,
    Timestamp           = 0x00000004,
    ForceUpdateAS       = 0x00000008,
};

enum class RenderFlag : uint32_t
{
    CullBackFaces       = 0x00000001,
    AdaptiveSampling    = 0x00000002,
};

enum class MeshFlag : uint32_t
{
    HasBlendshapes  = 0x00000001,
    HasJoints       = 0x00000002,
    IsDynamic       = 0x00000004,
};

enum class InstanceFlag : uint32_t
{
    ReceiveShadows  = 0x00000001,
    ShadowsOnly     = 0x00000002,
    CastShadows     = 0x00000004,
    CullFront       = 0x00000010,
    CullBack        = 0x00000020,
    CullFrontShadow = 0x00000040,
    CullBackShadow  = 0x00000080,

    Default = ReceiveShadows | CastShadows | CullBack,
};

enum class DirtyFlag : uint32_t
{
    None        = 0x00000000,
    Transform   = 0x00000001,
    Blendshape  = 0x00000002,
    Joints      = 0x00000004,

    Indices     = 0x00000010,
    Points      = 0x00000020,
    Normals     = 0x00000040,
    Tangents    = 0x00000080,
    UV          = 0x00000100,

    Camera      = 0x00010000,
    Light       = 0x00020000,
    RenderTarget= 0x00040000,
    Texture     = 0x00080000,
    TextureData = 0x00100000,
    Material    = 0x00200000,
    Mesh        = 0x00400000,
    Instance    = 0x00800000,
    Scene       = 0x01000000,

    Shape = Indices | Points,
    Vertices = Points | Normals | Tangents | UV,
    Deform = Blendshape | Joints,
    SceneEntities = Camera | Light | RenderTarget | Instance,
    Any = 0xffffffff,
};
inline DirtyFlag operator|(DirtyFlag a, DirtyFlag b) { return (DirtyFlag)((uint32_t)a | (uint32_t)b); }


int GetTexelSize(TextureFormat v);

#define DefCompare(T)\
    bool operator==(const T& v) const { return std::memcmp(this, &v, sizeof(*this)) == 0; }\
    bool operator!=(const T& v) const { return !(*this == v); }\

// these structs are directly uploaded to GPU buffers

struct CameraData
{
    float4x4 view = float4x4::identity();
    float4x4 proj = float4x4::identity();
    union {
        float3 position;
        float4 position4 = float4::zero();
    };
    quatf rotation = quatf::identity();

    float near_plane = 0.01f;
    float far_plane = 100.0f;
    float fov = 60.0f;
    float pad{};

    DefCompare(CameraData);
};

struct LightData
{
    LightType type = LightType::Directional;
    float3 position = float3::zero();
    float3 direction = -float3::up();
    float range = 1.0f;
    float3 color = float3::one();
    float spot_angle = 0.0f; // radian

    DefCompare(LightData);
};

struct MaterialData
{
    MaterialType type = MaterialType::Default;
    float3 diffuse = float3::one();
    float3 emissive = float3::zero();
    float roughness = 0.5f;
    float opacity = 1.0f;
    int diffuse_tex = -1;
    int emissive_tex = -1;
    int pad = 0;

    DefCompare(MaterialData);
};

struct BlendshapeFrameData
{
    int delta_offset = 0;
    float weight = 0;
};
struct BlendshapeData
{
    int frame_count = 0;
    int frame_offset = 0;
};
struct JointCount
{
    int weight_count = 0;
    int weight_offset = 0;
};

struct MeshData
{
    int face_count = 0;
    int index_count = 0;
    int vertex_count = 0;
    int flags = 0;
};

struct InstanceData
{
    float4x4 local_to_world = float4x4::identity();
    float4x4 world_to_local = float4x4::identity();
    int mesh_id = -1;
    int deform_id = -1;
    int instance_flags = 0; // combination of InstanceFlags
    int layer_mask = 0;
    int material_ids[32]{};

    DefCompare(InstanceData);
};

struct SceneData
{
    int frame = 0;
    int samples_per_frame = 16;
    int max_trace_depth = 8;
    int render_flags = 0; // combination of RenderFlag
    int light_count = 0;
    float3 bg_color = {0.1f, 0.1f, 0.1f};

    CameraData camera;
    LightData lights[lptMaxLights];

    DefCompare(SceneData);
    template<class Body>
    void eachLight(const Body& body)
    {
        for (int li = 0; li < light_count; ++li)
            body(lights[li]);
    }
};

struct vertex_t
{
    float3 point;
    float3 normal;
    float3 tangent;
    float2 uv;
    float pad;

    DefCompare(vertex_t);
};

struct face_t
{
    int3 indices;
    int material_index;
    float3 normal;
    float pad;

    DefCompare(face_t);
};

#undef DefCompare


template<class T>
class RefCount : public T
{
public:
    int getID() const override
    {
        return m_id;
    }

    int addRef() override
    {
        return ++m_ref_external;
    }
    int release() override
    {
        int ret = --m_ref_external;
        if (ret == 0)
            onRefCountZero();
        return ret;
    }
    int getRef() const override
    {
        return m_ref_external;
    }

    virtual void onRefCountZero()
    {
        // not delete by default. deleting entities is responsible for Context.
        // (ContextDXR::frameBegin() etc.)
    }

    int addRefInternal()
    {
        return ++m_ref_internal;
    }
    int releaseInternal()
    {
        int ret = --m_ref_internal;
        if (ret == 0)
            delete this;
        return ret;
    }
    int getRefInternal() const
    {
        return m_ref_internal;
    }

    void setName(const char* v) override
    {
        m_name = v;
    }

    const char* getName() const override
    {
        return m_name.c_str();
    }

public:
    std::atomic_int m_ref_external{ 0 };
    std::atomic_int m_ref_internal{ 0 };
    int m_id = 0;
    std::string m_name;
};


template<class T>
class EntityBase : public RefCount<T>
{
public:
    bool isDirty(DirtyFlag v = DirtyFlag::Any) const
    {
        return get_flag(m_dirty_flags, v);
    }

    void markDirty(DirtyFlag v)
    {
        set_flag(m_dirty_flags, v, true);
    }

    void clearDirty()
    {
        m_dirty_flags = 0;
    }

public:
    uint32_t m_dirty_flags = 0;
};


class Globals : public IGlobals
{
public:
    static Globals& getInstance();

    void enableStrictUpdateCheck(bool v) override;
    void enablePowerStableState(bool v) override;
    void enableTimestamp(bool v) override;
    void enableForceUpdateAS(bool v) override;
    void setSamplesPerFrame(int v) override;
    void setMaxTraceDepth(int v) override;

    bool isStrictUpdateCheckEnabled() const;
    bool isPowerStableStateEnabled() const;
    bool isTimestampEnabled() const;
    bool isForceUpdateASEnabled() const;
    int getSamplesPerFrame() const;
    int getMaxTraceDepth() const;

private:
    Globals();
    ~Globals() override;
    void setFlag(GlobalFlag f, bool v);
    bool getFlag(GlobalFlag f) const;

    uint32_t m_flags = 0;
    int m_samples_per_frame = 16;
    int m_max_trace_depth = 8;
};


#define lptDefBaseT(T, I)\
    inline T* base_t(I* v) { return static_cast<T*>(v); }\
    inline T& base_t(I& v) { return static_cast<T&>(v); }


class Texture : public EntityBase<ITexture>
{
public:
    Texture(TextureFormat format, int width, int height);
    void upload(const void* src) override;

public:
    TextureFormat m_format = TextureFormat::RGBAu8;
    int m_width = 0;
    int m_height = 0;
    RawVector<char> m_data;
};
lptDefRefPtr(Texture);
lptDefBaseT(Texture, ITexture)


class RenderTarget : public EntityBase<IRenderTarget>
{
public:
    RenderTarget(TextureFormat format, int width, int height);
    void enableReadback(bool v) override;

public:
    TextureFormat m_format = TextureFormat::RGBAu8;
    int m_width = 0;
    int m_height = 0;
    bool m_readback_enabled = false;
};
lptDefRefPtr(RenderTarget);
lptDefBaseT(RenderTarget, IRenderTarget)


class Material : public EntityBase<IMaterial>
{
public:
    Material();
    void setType(MaterialType v) override;
    void setDiffuse(float3 v) override;
    void setRoughness(float v) override;
    void setEmissive(float3 v) override;
    void setDiffuseTexture(ITexture* v) override;
    void setEmissiveTexture(ITexture* v) override;

public:
    MaterialData m_data;
    TexturePtr m_tex_diffuse;
    TexturePtr m_tex_emissive;
};
lptDefRefPtr(Material);
lptDefBaseT(Material, IMaterial)


class Camera : public EntityBase<ICamera>
{
public:
    Camera();
    void setPosition(float3 v) override;
    void setDirection(float3 v, float3 up) override;
    void setFOV(float v) override;
    void setNear(float v) override;
    void setFar(float v) override;

public:
    CameraData m_data;
};
lptDefRefPtr(Camera);
lptDefBaseT(Camera, ICamera)


class Light : public EntityBase<ILight>
{
public:
    Light();
    void setType(LightType v) override;
    void setPosition(float3 v) override;
    void setDirection(float3 v) override;
    void setRange(float v) override;
    void setSpotAngle(float v) override;
    void setColor(float3 v) override;

public:
    LightData m_data;
};
lptDefRefPtr(Light);
lptDefBaseT(Light, ILight)


class Mesh;

class Blendshape : public IBlendshape
{
public:
    struct Frame
    {
        RawVector<float3> delta_points;
        RawVector<float3> delta_normals;
        RawVector<float3> delta_tangents;
        RawVector<float2> delta_uv;
        float weight = 1.0f;
    };
    using FramePtr = std::shared_ptr<Frame>;

    Blendshape(Mesh* mesh);
    void setName(const char* name) override;
    int addFrame() override;
    void setWeight(int frame, float v) override;
    void setDeltaPoints(int frame, const float3* v, size_t n) override;
    void setDeltaNormals(int frame, const float3* v, size_t n) override;
    void setDeltaTangents(int frame, const float3* v, size_t n) override;
    void setDeltaUV(int frame, const float2* v, size_t n) override;

    int getFrameCount() const;
    void exportDelta(int frame, vertex_t* dst) const;


    // Body: [](const Blendshape::Frame&) -> void
    template<class Body>
    void eachFrame(const Body& body) const
    {
        for (auto& pf : m_frames)
            body(*pf);
    }

public:
    Mesh* m_mesh = nullptr;
    std::string m_name;
    std::vector<FramePtr> m_frames;
};
using BlendshapePtr = std::shared_ptr<Blendshape>;

class Mesh : public EntityBase<IMesh>
{
public:
    void setIndices(const int* v, size_t n) override;
    void setPoints(const float3* v, size_t n) override;
    void setNormals(const float3* v, size_t n) override;
    void setTangents(const float3* v, size_t n) override;
    void setUV(const float2* v, size_t n) override;
    void setMaterialIDs(const int* v, size_t n) override;

    void setJointBindposes(const float4x4* v, size_t n) override;
    void setJointWeights(const JointWeight* v, size_t n) override;
    void setJointCounts(const int* v, size_t n) override;
    void clearJoints() override;
    IBlendshape* addBlendshape() override;
    void clearBlendshapes() override;
    void markDynamic() override;

    bool hasBlendshapes() const;
    bool hasJoints() const;
    bool isDynamic() const;
    int getFaceCount() const;
    int getIndexCount() const;
    int getVertexCount() const;
    void updateFaceData();
    void exportVertices(vertex_t* dst) const;
    void exportFaces(face_t* dst);

    int getJointCount() const;
    int getJointWeightCount() const;
    void exportJointCounts(JointCount* dst) const;
    void exportJointWeights(JointWeight* dst) const;

    int getBlendshapeCount() const;
    int getBlendshapeFrameCount() const;
    void exportBlendshapes(BlendshapeData* dst) const;
    void exportBlendshapeFrames(BlendshapeFrameData* dst) const;
    void exportBlendshapeDelta(vertex_t* dst) const;

    // Body: [](const Blendshape&) -> void
    template<class Body>
    void eachBlendshape(const Body& body) const
    {
        for (auto& pbs : m_blendshapes)
            body(*pbs);
    }

public:
    MeshData m_data;
    RawVector<int>    m_indices;
    RawVector<float3> m_points;
    RawVector<float3> m_normals;  // per-vertex
    RawVector<float3> m_tangents; // 
    RawVector<float2> m_uv;       // 
    RawVector<float3> m_face_normals; // per-face
    RawVector<int>    m_material_ids; // 

    RawVector<float4x4>    m_joint_bindposes;
    RawVector<int>         m_joint_counts;
    RawVector<JointWeight> m_joint_weights;

    std::vector<BlendshapePtr> m_blendshapes;
};
lptDefRefPtr(Mesh);
lptDefBaseT(Mesh, IMesh)


class MeshInstance : public EntityBase<IMeshInstance>
{
public:
    MeshInstance(IMesh* v = nullptr);
    void setMaterial(IMaterial* v, int slot) override;
    void setTransform(const float4x4& v) override;
    void setJointMatrices(const float4x4* v) override;
    bool hasFlag(InstanceFlag flag) const;

    void exportJointMatrices(float4x4* dst) const;
    void exportBlendshapeWeights(float* dst) const;

public:
    InstanceData m_data;
    MeshPtr m_mesh;
    MaterialPtr m_material;
    RawVector<float4x4> m_joint_matrices;
    RawVector<float> m_blendshape_weights;

    uint32_t m_instance_flags = (uint32_t)InstanceFlag::Default;
};
lptDefRefPtr(MeshInstance);
lptDefBaseT(MeshInstance, IMeshInstance)



class Scene : public EntityBase<IScene>
{
public:
    void setEnabled(bool v) override;
    void setBackgroundColor(float3 v) override;

    void setRenderTarget(IRenderTarget* v) override;
    void setCamera(ICamera* v) override;
    void addLight(ILight* v) override;
    void removeLight(ILight* v) override;
    void addMesh(IMeshInstance* v) override;
    void removeMesh(IMeshInstance* v) override;
    void clear() override;

    void updateData();

public:
    bool m_enabled = true;
    SceneData m_data;
    CameraPtr m_camera;
    RenderTargetPtr m_render_target;
    std::vector<LightPtr> m_lights;
    std::vector<MeshInstancePtr> m_instances;
};
lptDefRefPtr(Scene);
lptDefBaseT(Scene, IScene)


class Context : public RefCount<IContext>
{
public:
    void onRefCountZero() override
    {
        delete this;
    }

public:
};
lptDefRefPtr(Context);
lptDefBaseT(Context, IContext)




} // namespace lpt
