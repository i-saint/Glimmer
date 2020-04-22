#pragma once
#include "lptSettings.h"
#include "Foundation/lptRefPtr.h"
#include "Foundation/lptMath.h"
#include "Foundation/lptHalf.h"
#include "lptInterface.h"

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
    float3 diffuse;
    float roughness;
    float3 emissive;
    float opacity;
    int diffuse_tex;
    int emissive_tex;
    int2 pad_tex;
};

#define kMaxLights 32

struct SceneData
{
    uint32_t render_flags; // combination of RenderFlag
    uint32_t light_count;
    float shadow_ray_offset;
    float self_shadow_threshold;

    CameraData camera;
    LightData lights[kMaxLights];

    bool operator==(SceneData& v) const { return std::memcmp(this, &v, sizeof(*this)) == 0; }
    bool operator!=(SceneData& v) const { return !(*this == v); }

    template<class Body>
    void eachLight(const Body& body)
    {
        for (uint32_t li = 0; li < light_count; ++li)
            body(lights[li]);
    }
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


using GPUResourcePtr = const void*;
using CPUResourcePtr = const void*;

class DeviceMeshData
{
public:
    virtual ~DeviceMeshData() {};
    virtual bool valid() const = 0;
};

class DeviceMeshInstanceData
{
public:
    virtual ~DeviceMeshInstanceData() {};
    virtual bool valid() const = 0;
};

class DeviceRenderTargetData
{
public:
    virtual ~DeviceRenderTargetData() {};
    virtual bool valid() const = 0;
};


struct SkinData
{
    std::vector<float4x4>   bindposes;
    std::vector<uint8_t>    bone_counts;
    std::vector<JointWeight> weights;

    bool valid() const;
};

struct BlendshapeFrameData
{
    std::vector<float3> delta;
    float weight = 0.0f;
};
struct BlendshapeData
{
    std::vector<BlendshapeFrameData> frames;
};


class MeshData : public SharedResource<MeshData>
{
public:
    std::string name;
    GPUResourcePtr gpu_vertex_buffer = nullptr;
    GPUResourcePtr gpu_index_buffer = nullptr;
    CPUResourcePtr cpu_vertex_buffer = nullptr;
    CPUResourcePtr cpu_index_buffer = nullptr;
    int vertex_stride = 0; // if 0, treated as size_of_vertex_buffer / vertex_count
    int vertex_count = 0;
    int vertex_offset = 0; // in byte
    int index_stride = 0;
    int index_count = 0;
    int index_offset = 0; // in byte
    SkinData skin;
    std::vector<BlendshapeData> blendshapes;
    bool is_dynamic = false;

    DeviceMeshData *device_data = nullptr;

    MeshData();
    ~MeshData();
    void release();
    bool valid() const;
};
using MeshDataPtr = ref_ptr<MeshData>;


class MeshInstanceData : public SharedResource<MeshInstanceData>
{
public:
    std::string name;
    MeshDataPtr mesh;
    float4x4 transform = float4x4::identity();
    std::vector<float4x4> bones;
    std::vector<float> blendshape_weights;
    uint32_t update_flags = 0; // combination of UpdateFlag

    uint32_t layer = 0;
    uint32_t layer_mask = 0;
    uint32_t instance_flags = (uint32_t)InstanceFlag::Default;

    DeviceMeshInstanceData *device_data = nullptr;

    MeshInstanceData();
    ~MeshInstanceData();
    void release();
    bool valid() const;
    bool isUpdated(UpdateFlag v) const;
    void clearUpdateFlags();
    void markUpdated(UpdateFlag v);
    void markUpdated(); // for debug
    bool hasFlag(InstanceFlag flag) const;
    void setTransform(const float4x4& v);
    void setBones(const float4x4 *v, size_t n);
    void setBlendshapeWeights(const float *v, size_t n);
    void setFlags(uint32_t v);
    void setLayer(uint32_t v);
};
using MeshInstanceDataPtr = ref_ptr<MeshInstanceData>;


class RenderTargetData : public SharedResource<RenderTargetData>
{
public:
    std::string name;
    GPUResourcePtr gpu_texture = nullptr;
    int width = 0;
    int height = 0;
    TextureFormat format = TextureFormat::Unknown;

    DeviceRenderTargetData *device_data = nullptr;


    RenderTargetData();
    ~RenderTargetData();
    void release();
};
using RenderTargetDataPtr = ref_ptr<RenderTargetData>;



template<class T>
inline void ExternalRelease(T *self)
{
    self->internalRelease();
}

} // namespace lpt
