#pragma once
#include "MeshUtils/MeshUtils.h"
#include "Foundation/gptUtils.h"

#define gptDefRefPtr(T) using T##Ptr = ref_ptr<T, InternalReleaser<T>>

namespace gpt {

enum class GlobalFlag : uint32_t
{
    GenerateTangents    = 0x00000001,
    StrictUpdateCheck   = 0x00000002,
    Timestamp           = 0x00000004,
    PowerStableState    = 0x00000008,
    ForceUpdateAS       = 0x00000010,
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

enum class DirtyFlag : uint32_t
{
    None        = 0x00000000,
    Transform   = 0x00000001,
    Blendshape  = 0x00000002,
    Joints      = 0x00000004,
    Flags       = 0x00000008,

    Indices     = 0x00000100,
    Points      = 0x00000200,
    Normals     = 0x00000400,
    Tangents    = 0x00000800,
    UV          = 0x00001000,

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

enum LayerMask : uint32_t
{
    Visible     = 0x01,
    Shadow      = 0x02,
    LightSource = 0x04,
    Opaque      = 0x08,
    Transparent = 0x10,
    Translucent = 0x20,
};
inline LayerMask operator|(LayerMask a, LayerMask b) { return (LayerMask)((uint32_t)a | (uint32_t)b); }


int GetTexelSize(Format v);

#define gptDefCompare(T)\
    bool operator==(const T& v) const { return std::memcmp(this, &v, sizeof(*this)) == 0; }\
    bool operator!=(const T& v) const { return !(*this == v); }\

#define gptAssertAlign16(T)\
    static_assert(sizeof(CameraData) % 16 == 0, "not 16 byte aligned")

// these structs are directly uploaded to GPU buffers

struct CameraData
{
    float4x4 view = float4x4::identity();
    float4x4 proj = float4x4::identity();
    float4x4 viewproj = float4x4::identity();
    float3   position = float3::zero();
    float    fov = 60.0f;
    quatf    rotation = quatf::identity();
    float    near_plane = 0.01f;
    float    far_plane = 100.0f;
    int2    screen_size = { 0, 0 };

    gptDefCompare(CameraData);
};
gptAssertAlign16(CameraData);

struct LightData
{
    LightType type = LightType::Directional;
    float3 position = float3::zero();
    float3 direction = -float3::up();
    float range = 100.0f;
    float3 color = float3::one();
    float intensity = 1.0f;
    float spot_angle = 0.0f; // radian
    float disperse = 0.1f;
    int mesh_instance_id = -1; // for mesh light
    int pad{};

    gptDefCompare(LightData);
};
gptAssertAlign16(LightData);

struct MaterialData
{
    float3 diffuse{ 0.8f, 0.8f, 0.8f };
    MaterialType type = MaterialType::Opaque;
    float3 emissive{ 0.0f, 0.0f, 0.0f };
    float roughness = 0.5f;
    float3 rimlight_color{ 0.0f, 0.0f, 0.0f };
    float rimlight_falloff = 2.0f;
    float opacity = 1.0f;
    float fresnel = 0.9f;
    float refraction_index = 1.0f;
    int diffuse_tex = -1;
    int opacity_tex = -1;
    int roughness_tex = -1;
    int emissive_tex = -1;
    int normal_tex = -1;
    float4x4 portal_transform = float4x4::identity();
    int portal_scene = 0;
    int3 pad;

    gptDefCompare(MaterialData);
};
gptAssertAlign16(MaterialData);

struct BlendshapeData
{
    int frame_count = 0;
    int frame_offset = 0;
};
struct BlendshapeFrameData
{
    float weight = 0;
    int delta_offset = 0;
};
struct JointCount
{
    int weight_count = 0;
    int weight_offset = 0;
};

struct MeshData
{
    int vb_id = -1;
    int ib_id = -1;
    int vertex_count = 0;
    int triangle_count = 0;
    int flags = 0;
    int3 pad{};
};
gptAssertAlign16(MeshData);

struct InstanceData
{
    float4x4 transform = float4x4::identity();
    float4x4 itransform = float4x4::identity();
    int vb_id = -1;
    int ib_id = -1;
    int mesh_id = -1;
    int material_id = 0; // 0: default material
    float2 uv_offset{ 0.0f, 0.0f };
    int instance_flags = (int)InstanceFlag::Default;
    int pad{};

    gptDefCompare(InstanceData);
};
gptAssertAlign16(InstanceData);

struct SceneData
{
    int frame = 0;
    int samples_per_frame = 16;
    int max_trace_depth = 8;
    int light_count = 0;
    int meshlight_count = 0;
    float3 bg_color = {0.1f, 0.1f, 0.1f};
    float time = 0.0f;
    float3 pad;

    CameraData camera;
    CameraData camera_prev;

    gptDefCompare(SceneData);
};
gptAssertAlign16(SceneData);

struct vertex_t
{
    float3 point;
    float3 normal;
    float3 tangent;
    float2 uv;
    float pad;

    gptDefCompare(vertex_t);
};

#undef gptDefCompare


template<class T>
class RefCount : public T
{
public:
    // forbid copy
    RefCount(const RefCount& v) = delete;
    RefCount& operator=(const RefCount& v) = delete;

    RefCount() {}

    void setID(int v)
    {
        m_id = v;
    }

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

protected:
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

protected:
    uint32_t m_dirty_flags = 0;
};

class MeshInstance;
class Scene;
gptDefRefPtr(MeshInstance);
gptDefRefPtr(Scene);



class Globals : public IGlobals
{
public:
    static Globals& getInstance();

    void enableGenerateTangents(bool v) override;
    void enableStrictUpdateCheck(bool v) override;
    void enableTimestamp(bool v) override;
    void enablePowerStableState(bool v) override;
    void enableForceUpdateAS(bool v) override;
    void setSamplesPerFrame(int v) override;
    void setMaxTraceDepth(int v) override;

    bool isGenerateTangentsEnabled() const;
    bool isStrictUpdateCheckEnabled() const;
    bool isTimestampEnabled() const;
    bool isPowerStableStateEnabled() const;
    bool isForceUpdateASEnabled() const;
    int getSamplesPerFrame() const;
    int getMaxTraceDepth() const;

private:
    Globals();
    ~Globals() override;
    void setFlag(GlobalFlag f, bool v);
    bool getFlag(GlobalFlag f) const;

    uint32_t m_flags = 0;
    int m_samples_per_frame = 1;
    int m_max_trace_depth = 4;
};


#define gptDefBaseT(T, I)\
    inline T* base_t(I* v) { return static_cast<T*>(v); }\
    inline T& base_t(I& v) { return static_cast<T&>(v); }


class Texture : public EntityBase<ITexture>
{
public:
    Texture(int width, int height, Format format);
    void upload(const void* src) override;
    int getWidth() const override;
    int getHeight() const override;
    Format getFormat() const override;
    Span<char> getData() const override;

protected:
    int m_width = 0;
    int m_height = 0;
    Format m_format = Format::RGBAu8;
    RawVector<char> m_data;
};
gptDefRefPtr(Texture);
gptDefBaseT(Texture, ITexture)


class Window;

class RenderTarget : public EntityBase<IRenderTarget>
{
public:
    RenderTarget(int width, int height, Format format);
    RenderTarget(IWindow* window, Format format);
    void enableReadback(bool v) override;
    int getWidth() const override;
    int getHeight() const override;
    Format getFormat() const override;
    IWindow* getWindow() const override;

protected:
    int m_width = 0;
    int m_height = 0;
    Format m_format = Format::RGBAu8;
    Window* m_window = nullptr;
    bool m_readback_enabled = false;
};
gptDefRefPtr(RenderTarget);
gptDefBaseT(RenderTarget, IRenderTarget)


class Material : public EntityBase<IMaterial>
{
public:
    Material();
    void setType(MaterialType v) override;
    void setDiffuse(float3 v) override;
    void setRoughness(float v) override;
    void setEmissive(float3 v) override;
    void setRefractionIndex(float v) override;
    void setOpacity(float v) override;
    void setFresnel(float v) override;
    void setRimLightColor(float3 v) override;
    void setRimLightFalloff(float v) override;
    void setDiffuseMap(ITexture* v) override;
    void setOpacityMap(ITexture* v) override;
    void setRoughnessMap(ITexture* v) override;
    void setEmissiveMap(ITexture* v) override;
    void setNormalMap(ITexture* v) override;
    void setPortalTransform(float4x4 v) override;
    void setPortalScene(IScene* v) override;

    MaterialType getType() const override;
    float3       getDiffuse() const override;
    float        getRoughness() const override;
    float3       getEmissive() const override;
    float        getRefractionIndex() const override;
    float        getOpacity() const override;
    float        getFresnel() const override;
    float3       getRimLightColor() const override;
    float        getRimLightFalloff() const override;
    ITexture*    getDiffuseMap() const override;
    ITexture*    getOpacityMap() const override;
    ITexture*    getRoughnessMap() const override;
    ITexture*    getEmissiveMap() const override;
    ITexture*    getNormalMap() const override;
    float4x4     getPortalTransform() const override;
    IScene*      getPortalScene() const override;

    const MaterialData& getData();

protected:
    MaterialData m_data;
    TexturePtr m_tex_diffuse;
    TexturePtr m_tex_opacity;
    TexturePtr m_tex_roughness;
    TexturePtr m_tex_emissive;
    TexturePtr m_tex_normal;
    ScenePtr m_portal_scene;
};
gptDefRefPtr(Material);
gptDefBaseT(Material, IMaterial)


class Camera : public EntityBase<ICamera>
{
public:
    Camera();
    void setPosition(float3 v) override;
    void setDirection(float3 v, float3 up) override;
    void setFOV(float v) override;
    void setNear(float v) override;
    void setFar(float v) override;
    void setRenderTarget(IRenderTarget* v) override;

    float3 getPosition() const override;
    float3 getDirection() const override;
    float getFOV() const override;
    float getNear() const override;
    float getFar() const override;
    IRenderTarget* getRenderTarget() const override;

    const CameraData& getData();

protected:
    CameraData m_data;
    RenderTargetPtr m_render_target;
};
gptDefRefPtr(Camera);
gptDefBaseT(Camera, ICamera)

class Light : public EntityBase<ILight>
{
public:
    Light();
    ~Light();
    void setEnabled(bool v) override;
    void setType(LightType v) override;
    void setPosition(float3 v) override;
    void setDirection(float3 v) override;
    void setRange(float v) override;
    void setSpotAngle(float v) override;
    void setColor(float3 v) override;
    void setIntensity(float v) override;
    void setDisperse(float v) override;
    void setMesh(IMeshInstance* v) override;

    bool      isEnabled() const override;
    LightType getType() const override;
    float3    getPosition() const override;
    float3    getDirection() const override;
    float     getRange() const override;
    float     getSpotAngle() const override;
    float3    getColor() const override;
    float     getIntensity() const override;
    float     getDisperse() const override;
    IMeshInstance* getMesh() const override;

    const LightData& getData();

protected:
    bool m_enabled = true;
    LightData m_data;
    MeshInstancePtr m_mesh;
};
gptDefRefPtr(Light);
gptDefBaseT(Light, ILight)


class Mesh;

class BlendshapeFrame : public IBlendshapeFrame
{
public:
    BlendshapeFrame(float weight);
    void setDeltaPoints(const float3* v, size_t n) override;
    void setDeltaNormals(const float3* v, size_t n) override;
    void setDeltaTangents(const float3* v, size_t n) override;
    void setDeltaUV(const float2* v, size_t n) override;
    Span<float3> getDeltaPoints() const override;
    Span<float3> getDeltaNormals() const override;
    Span<float3> getDeltaTangents() const override;
    Span<float2> getDeltaUV() const override;

public:
    RawVector<float3> m_delta_points;
    RawVector<float3> m_delta_normals;
    RawVector<float3> m_delta_tangents;
    RawVector<float2> m_delta_uv;
    float m_weight = 1.0f;
};
using BlendshapeFramePtr = std::shared_ptr<BlendshapeFrame>;

class Blendshape : public IBlendshape
{
public:

    Blendshape(Mesh* mesh);
    void setName(const char* name) override;
    int getFrameCount() const override;
    IBlendshapeFrame* getFrame(int i) override;
    IBlendshapeFrame* addFrame(float weight) override;
    void removeFrame(IBlendshapeFrame* f) override;

    void exportDelta(int frame, vertex_t* dst) const;


    // Body: [](const Blendshape::Frame&) -> void
    template<class Body>
    void eachFrame(const Body& body) const
    {
        for (auto& pf : m_frames)
            body(*pf);
    }

protected:
    Mesh* m_mesh = nullptr;
    std::string m_name;
    std::vector<BlendshapeFramePtr> m_frames;
};
using BlendshapePtr = std::shared_ptr<Blendshape>;


class Mesh : public EntityBase<IMesh>
{
public:
    void setPoints(const float3* v, size_t n) override;
    void setNormals(const float3* v, size_t n) override;
    void setTangents(const float3* v, size_t n) override;
    void setUV(const float2* v, size_t n) override;
    Span<float3> getPoints() const override;
    Span<float3> getNormals() const override;
    Span<float3> getTangents() const override;
    Span<float2> getUV() const override;

    void setIndices(const int* v, size_t n) override;
    Span<int> getIndices() const override;

    void markDynamic() override;

    void setJointBindposes(const float4x4* v, size_t n) override;
    void setJointWeights(const JointWeight* v, size_t n) override;
    void setJointCounts(const int* v, size_t n) override;
    Span<float4x4>    getJointBindposes() const override;
    Span<JointWeight> getJointWeights() const override;
    Span<int>         getJointCounts() const override;

    int getBlendshapeCount() const override;
    IBlendshape* getBlendshape(int i) override;
    IBlendshape* addBlendshape() override;
    void removeBlendshape(IBlendshape* f) override;

    virtual void update();

    bool hasBlendshapes() const;
    bool hasJoints() const;
    bool isDynamic() const;
    int getFaceCount() const;
    int getIndexCount() const;
    int getVertexCount() const;
    void exportVertices(vertex_t* dst) const;

    int getJointCount() const;
    int getJointWeightCount() const;
    void exportJointCounts(JointCount* dst) const;
    void exportJointWeights(JointWeight* dst) const;

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

    const MeshData& getData();

public:
    MeshData m_data;
    RawVector<float3> m_points;
    RawVector<float3> m_normals;  // per-vertex
    RawVector<float3> m_tangents; // 
    RawVector<float2> m_uv;       // 
    RawVector<int> m_indices;

    RawVector<float4x4>    m_joint_bindposes;
    RawVector<int>         m_joint_counts;
    RawVector<JointWeight> m_joint_weights;

    std::vector<BlendshapePtr> m_blendshapes;
};
gptDefRefPtr(Mesh);
gptDefBaseT(Mesh, IMesh)


class MeshInstance : public EntityBase<IMeshInstance>
{
public:
    MeshInstance(IMesh* v = nullptr);
    void setEnabled(bool v) override;
    void setFlag(InstanceFlag f, bool v) override;
    void setMaterial(IMaterial* v) override;
    void setTransform(float4x4 v) override;
    void setJointMatrices(const float4x4* v) override;
    void setBlendshapeWeights(const float* v) override;

    IMesh*          getMesh() const override;
    bool            isEnabled() const override;
    bool            getFlag(InstanceFlag f) const override;
    IMaterial*      getMaterial() const override;
    float4x4        getTransform() const override;
    Span<float4x4>  getJointMatrices() const override;
    Span<float>     getBlendshapeWeights() const override;

    virtual void update();

    void addLightSourceCount(bool v);
    bool isLightSource() const;
    void exportJointMatrices(float4x4* dst) const;
    void exportBlendshapeWeights(float* dst) const;
    const InstanceData& getData();

public:
    bool m_enabled = true;
    InstanceData m_data;
    MeshPtr m_mesh;
    MaterialPtr m_material;
    RawVector<float4x4> m_joint_matrices;
    RawVector<float> m_blendshape_weights;

    int m_light_source_count = 0;
    uint32_t m_instance_flags = (uint32_t)InstanceFlag::Default;
};
gptDefRefPtr(MeshInstance);
gptDefBaseT(MeshInstance, IMeshInstance)



class Scene : public EntityBase<IScene>
{
public:
    void            setEnabled(bool v) override;
    void            setBackgroundColor(float3 v) override;
    bool            isEnabled() const override;
    float3          getBackgroundColor() const override;

    int             getCameraCount() const override;
    ICamera*        getCamera(int i) const override;
    void            addCamera(ICamera* v) override;
    void            removeCamera(ICamera* v) override;

    int             getLightCount() const override;
    ILight*         getLight(int i) const override;
    void            addLight(ILight* v) override;
    void            removeLight(ILight* v) override;

    int             getInstanceCount() const override;
    IMeshInstance*  getInstance(int i) const override;
    void            addInstance(IMeshInstance* v) override;
    void            removeInstance(IMeshInstance* v) override;

    virtual void update();

    const SceneData& getData();
    void incrementFrameCount();

protected:
    bool m_enabled = true;
    SceneData m_data;
    mu::Timer m_timer;
    std::vector<CameraPtr> m_cameras;
    std::vector<LightPtr> m_lights;
    std::vector<MeshInstancePtr> m_instances;
};
gptDefRefPtr(Scene);
gptDefBaseT(Scene, IScene)


class Context : public RefCount<IContext>
{
public:
    void onRefCountZero() override
    {
        delete this;
    }

public:
};
gptDefRefPtr(Context);
gptDefBaseT(Context, IContext)




} // namespace gpt
