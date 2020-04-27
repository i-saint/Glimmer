#define kMaxLights 32

enum LightType
{
    LT_DIRECTIONAL,
    LT_POINT,
    LT_SPOT,
};

enum MaterialType
{
    MT_DEFAULT,
};

enum RayType
{
    RT_RADIANCE,
    RT_OCCLUSION,
};

enum RenderFlag
{
    RF_CULL_BACK_FACES = 0x00000001,
    RF_FLIP_CASTER_FACES = 0x00000002,
    RF_IGNORE_SELF_SHADOW = 0x00000004,
    RF_KEEP_SELF_DROP_SHADOW = 0x00000008,
};

enum InstanceFlag
{
    IF_RECEIVE_SHADOWS = 0x01,
    IF_SHADOWS_ONLY = 0x02,
    IF_CAST_SHADOWS = 0x04,
};

enum DeformFlag
{
    DF_APPLY_BLENDSHAPE = 1,
    DF_APPLY_SKINNING = 2,
};


struct CameraData
{
    float4x4 view;
    float4x4 proj;
    float4 position;
    float4 rotation;
    float near_plane;
    float far_plane;
    float fov;
    float pad;
};

struct LightData
{
    uint type; // LightType
    float3 position;
    float3 direction;
    float range;
    float3 color;
    float spot_angle; // radian
};

struct MaterialData
{
    uint type; // MaterialType
    float3 diffuse;
    float3 emissive;
    float roughness;
    float opacity;
    int diffuse_tex;
    int emissive_tex;
    float pad;
};

struct MeshData
{
    uint face_count;
    uint index_count;
    uint vertex_count;
    uint flags; // DeformFlag
};

struct InstanceData
{
    float4x4 local_to_world;
    float4x4 world_to_local;
    uint mesh_index;
    uint material_index;
    uint instance_flags;
    uint layer_mask;
};

struct SceneData
{
    uint render_flags;
    uint sample_count;
    uint light_count;
    uint pad1;
    float3 bg_color;
    float pad2;

    CameraData camera;
    LightData lights[kMaxLights];
};

struct vertex_t
{
    float3 position;
    float3 normal;
    float3 tangent;
    float2 uv;
    float pad;
};

struct face_t
{
    int3 indices;
    float3 normal;
    float2 pad;
};


struct BlendshapeFrame
{
    uint delta_offset;
    float weight;
};
struct BlendshapeInfo
{
    uint frame_count;
    uint frame_offset;
};

struct BoneCount
{
    uint weight_count;
    uint weight_offset;
};
struct BoneWeight
{
    float weight;
    uint bone_index;
};


