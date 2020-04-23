#pragma once
#include "MeshUtils/MeshUtils.h"
#include "lptSettings.h"
#include "Foundation/lptMath.h"
#include "Foundation/lptHalf.h"
#include "lptInterface.h"

#define lptDeclRefPtr(T) using T##Ptr = ref_ptr<T>
#define lptMaxLights 32

namespace lpt {

enum class DebugFlag : uint32_t
{
    Timestamp       = 0x01,
    ForceUpdateAS   = 0x02,
    PowerStableState= 0x04,
};

enum class GlobalFlag : uint32_t
{
};

enum class RenderFlag : uint32_t
{
    CullBackFaces           = 0x00000001,
    FlipCasterFaces         = 0x00000002,
    IgnoreSelfShadow        = 0x00000004,
    KeepSelfDropShadow      = 0x00000008,
    AlphaTest               = 0x00000010,
    Transparent             = 0x00000020,
    AdaptiveSampling        = 0x00000100,
    Antialiasing            = 0x00000200,
    GPUSkinning             = 0x00010000,
    ClampBlendShapeWights   = 0x00020000,
};

enum class InstanceFlag : uint32_t
{
    ReceiveShadows  = 0x01,
    ShadowsOnly     = 0x02,
    CastShadows     = 0x04,
    CullFront       = 0x10,
    CullBack        = 0x20,
    CullFrontShadow = 0x40,
    CullBackShadow  = 0x80,

    Default = ReceiveShadows | CastShadows | CullBack,
};

enum class UpdateFlag : uint32_t
{
    None        = 0x00,
    Transform   = 0x01,
    Blendshape  = 0x02,
    Bones       = 0x04,
    Flags       = 0x08,

    Deform = Transform | Blendshape | Bones,
    Any = Transform | Blendshape | Bones | Flags,
};


// these structs are directly uploaded to GPU buffers

struct CameraData
{
    float4x4 view;
    float4x4 proj;
    union {
        float3 position;
        float4 position4;
    };
    float near_plane{};
    float far_plane{};
    uint32_t layer_mask{};
    uint32_t pad1;
};

struct LightData
{
    LightType light_type{};
    uint32_t layer_mask{};
    uint32_t pad1[2];

    float3 position{};
    float range{};
    float3 direction{};
    float spot_angle{}; // radian
    float3 color;
    float pad2;
};

struct MaterialData
{
    float3 diffuse{};
    float roughness{};
    float3 emissive{};
    float opacity{};

    int diffuse_tex{};
    int emissive_tex{};
    int2 pad_tex{};
};

struct SceneData
{
    uint32_t render_flags; // combination of RenderFlag
    uint32_t light_count;
    float shadow_ray_offset;
    float self_shadow_threshold;

    CameraData camera;
    LightData lights[lptMaxLights];

    bool operator==(SceneData& v) const { return std::memcmp(this, &v, sizeof(*this)) == 0; }
    bool operator!=(SceneData& v) const { return !(*this == v); }

    template<class Body>
    void eachLight(const Body& body)
    {
        for (uint32_t li = 0; li < light_count; ++li)
            body(lights[li]);
    }
};


template<class T>
class RefCount : public T
{
public:
    int addRef() override
    {
        return ++m_ref_count;
    }

    int release() override
    {
        int ret = --m_ref_count;
        if (ret == 0)
            delete this;
        return ret;
    }

    int getRef() const override
    {
        return m_ref_count;
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
    std::atomic_int m_ref_count{ 1 };
    std::string m_name;
};

class Camera : public RefCount<ICamera>
{
public:
    void setPosition(float3 v) override;
    void setRotation(quatf v) override;
    void setFOV(float v) override;
    void setNear(float v) override;
    void setFar(float v) override;

public:
    CameraData m_data;
};

class Light : public RefCount<ILight>
{
public:
    void setType(LightType v) override;
    void setPosition(float3 v) override;
    void setDirection(float3 v) override;
    void setRange(float v) override;
    void setSpotAngle(float v) override;
    void setColor(float3 v) override;

public:
    LightData m_data;
};


class Texture : public RefCount<ITexture>
{
public:
    bool setup(TextureFormat format, int width, int height) override;
    bool upload(const void* src) override;

public:
    int m_index = 0;
    TextureFormat m_format = TextureFormat::Unknown;
    int m_width = 0;
    int m_height = 0;
    RawVector<char> m_data;
};


class RenderTarget : public RefCount<IRenderTarget>
{
public:
    bool setup(TextureFormat format, int width, int height) override;
    bool readback(void* dst) override;

public:
    TextureFormat m_format = TextureFormat::Unknown;
    int m_width = 0;
    int m_height = 0;
    RawVector<char> m_data;
};


class Material : public RefCount<IMaterial>
{
public:
    void setDiffuse(float3 v) override;
    void setRoughness(float v) override;
    void setEmissive(float3 v) override;

    void setDiffuseTexture(ITexture* v) override;
    void setEmissionTexture(ITexture* v) override;

public:
    MaterialData m_data;
};


struct BlendshapeData
{
    struct FrameData
    {
        RawVector<float3> delta;
        float weight = 0.0f;
    };
    std::vector<FrameData> frames;
};

class Mesh : public RefCount<IMesh>
{
public:
    void setIndices(const int* v, size_t n) override;
    void setPoints(const float3* v, size_t n) override;
    void setNormals(const float3* v, size_t n) override;
    void setTangents(const float3* v, size_t n) override;
    void setUV(const float2* v, size_t n) override;

    void setJointBindposes(const float4x4* v, size_t n) override;
    void setJointWeights(const uint8_t* counts, size_t ncounts, const JointWeight* weights, size_t nweights) override;

protected:
    RawVector<int>    m_indices;
    RawVector<float3> m_points;
    RawVector<float3> m_normals;
    RawVector<float3> m_tangents;
    RawVector<float2> m_uv;

    RawVector<float4x4>    m_joint_bindposes;
    RawVector<uint8_t>     m_joint_counts;
    RawVector<JointWeight> m_joint_weights;

    std::vector<BlendshapeData> blendshapes;
};


class MeshInstance : public RefCount<IMeshInstance>
{
public:
    void setMesh(IMesh* v) override;
    void setMaterial(IMaterial* v) override;
    void setTransform(const float4x4& v) override;
    void setJointMatrices(const float4x4* v) override;

    bool isUpdated(UpdateFlag v) const;
    void clearUpdateFlags();
    bool hasFlag(InstanceFlag flag) const;

public:
    MeshPtr m_mesh;
    MaterialPtr m_material;
    float4x4 m_transform = float4x4::identity();
    RawVector<float4x4> m_joint_matrices;
    RawVector<float> m_blendshape_weights;

    uint32_t m_instance_flags = (uint32_t)InstanceFlag::Default;
    uint32_t m_update_flags = 0; // combination of UpdateFlag
};



class Scene : public RefCount<IScene>
{
public:
    void setRenderTarget(IRenderTarget* v) override;
    void setCamera(ICamera* v) override;
    void addLight(ILight* v) override;
    void addMesh(IMeshInstance* v) override;
    void clear() override;

public:
    SceneData m_data;
};


class Context : public RefCount<IContext>
{
public:
public:
};



struct GlobalSettings
{
    std::atomic_uint32_t debug_flags{ 0 }; // combination of DebugFlag
    std::atomic_uint32_t flags{ 0 }; // combination of GlobalFlag

    void enableDebugFlag(DebugFlag flag);
    void disableDebugFlag(DebugFlag flag);
    bool hasDebugFlag(DebugFlag flag) const;

    bool hasFlag(GlobalFlag v) const;
};

GlobalSettings& GetGlobals();

} // namespace lpt
