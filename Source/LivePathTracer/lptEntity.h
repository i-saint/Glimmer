#pragma once
#include "MeshUtils/MeshUtils.h"
#include "Foundation/lptUtils.h"

#define lptDeclRefPtr(T) using T##Ptr = ref_ptr<T, InternalReleaser<T>>
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
    float pad{};

    DefCompare(MaterialData);
};

struct MeshData
{
    uint32_t face_count = 0;
    uint32_t index_count = 0;
    uint32_t vertex_count = 0;
    uint32_t mesh_flags = 0;
};

struct InstanceData
{
    float4x4 local_to_world = float4x4::identity();
    float4x4 world_to_local = float4x4::identity();
    uint32_t mesh_index = -1;
    uint32_t material_index = -1;
    uint32_t instance_flags = 0; // combination of InstanceFlags
    uint32_t layer_mask = 0;

    DefCompare(InstanceData);
};

struct SceneData
{
    uint32_t frame = 0;
    uint32_t sample_count = 16;
    uint32_t render_flags = 0; // combination of RenderFlag
    uint32_t light_count = 0;
    float3 bg_color = {0.1f, 0.1f, 0.1f};
    float pad2{};

    CameraData camera;
    LightData lights[lptMaxLights];

    DefCompare(SceneData);
    template<class Body>
    void eachLight(const Body& body)
    {
        for (uint32_t li = 0; li < light_count; ++li)
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
        return (m_dirty_flags & (uint32_t)v) != 0;
    }

    void markDirty(DirtyFlag v)
    {
        m_dirty_flags |= (uint32_t)v;
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

    bool isStrictUpdateEnabled() const;
    bool isPowerStableStateEnabled() const;
    bool isTimestampEnabled() const;
    bool isForceUpdateASEnabled() const;

private:
    Globals();
    ~Globals() override;
    void setFlag(GlobalFlag f, bool v);
    bool getFlag(GlobalFlag f) const;

    uint32_t m_flags = 0;
};


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
lptDeclRefPtr(Texture);


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
lptDeclRefPtr(RenderTarget);


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
lptDeclRefPtr(Material);


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
lptDeclRefPtr(Camera);


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
lptDeclRefPtr(Light);


struct BlendshapeData
{
    struct FrameData
    {
        RawVector<float3> delta;
        float weight = 0.0f;
    };
    std::vector<FrameData> frames;
};

class Mesh : public EntityBase<IMesh>
{
public:
    void setIndices(const int* v, size_t n) override;
    void setPoints(const float3* v, size_t n) override;
    void setNormals(const float3* v, size_t n) override;
    void setTangents(const float3* v, size_t n) override;
    void setUV(const float2* v, size_t n) override;

    void setJointBindposes(const float4x4* v, size_t n) override;
    void setJointWeights(const JointWeight* v, size_t n) override;
    void setJointCounts(const uint8_t* v, size_t n) override;

    void markDynamic() override;

    size_t getVertexCount() const;
    size_t getFaceCount() const;
    void updateFaceNormals();
    void exportVertices(vertex_t* dst);
    void exportFaces(face_t* dst);

public:
    MeshData m_data;
    RawVector<int>    m_indices;
    RawVector<float3> m_face_normals;
    RawVector<float3> m_points;
    RawVector<float3> m_normals;
    RawVector<float3> m_tangents;
    RawVector<float2> m_uv;

    RawVector<float4x4>    m_joint_bindposes;
    RawVector<uint8_t>     m_joint_counts;
    RawVector<JointWeight> m_joint_weights;

    std::vector<BlendshapeData> blendshapes;

    bool m_dynamic = false;
};
lptDeclRefPtr(Mesh);


class MeshInstance : public EntityBase<IMeshInstance>
{
public:
    MeshInstance(IMesh* v = nullptr);
    void setMaterial(IMaterial* v) override;
    void setTransform(const float4x4& v) override;
    void setJointMatrices(const float4x4* v) override;
    bool hasFlag(InstanceFlag flag) const;

public:
    InstanceData m_data;
    MeshPtr m_mesh;
    MaterialPtr m_material;
    RawVector<float4x4> m_joint_matrices;
    RawVector<float> m_blendshape_weights;

    uint32_t m_instance_flags = (uint32_t)InstanceFlag::Default;
};
lptDeclRefPtr(MeshInstance);



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

    void update();

public:
    bool m_enabled = true;
    SceneData m_data;
    CameraPtr m_camera;
    RenderTargetPtr m_render_target;
    std::vector<LightPtr> m_lights;
    std::vector<MeshInstancePtr> m_instances;
};
lptDeclRefPtr(Scene);


class Context : public RefCount<IContext>
{
public:
    void onRefCountZero() override
    {
        delete this;
    }

public:
};
lptDeclRefPtr(Context);


#define lptDefBaseT(T, I)\
    class T;\
    inline T* base_t(I* v) { return static_cast<T*>(v); }\
    inline T& base_t(I& v) { return static_cast<T&>(v); }

lptDefBaseT(Camera, ICamera)
lptDefBaseT(Light, ILight)
lptDefBaseT(Texture, ITexture)
lptDefBaseT(RenderTarget, IRenderTarget)
lptDefBaseT(Material, IMaterial)
lptDefBaseT(Mesh, IMesh)
lptDefBaseT(MeshInstance, IMeshInstance)
lptDefBaseT(Scene, IScene)
lptDefBaseT(Context, IContext)

#undef lptDefBaseT

} // namespace lpt
