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
    int type; // LightType
    float3 position;
    float3 direction;
    float range;
    float3 color;
    float spot_angle; // radian
};

struct MaterialData
{
    int type; // MaterialType
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
    int face_count;
    int index_count;
    int vertex_count;
    int flags; // DeformFlag
};

struct InstanceData
{
    float4x4 local_to_world;
    float4x4 world_to_local;
    int mesh_id;
    int instance_flags;
    int layer_mask;
    int pad;
    int material_ids[32];

};

struct SceneData
{
    int frame;
    int samples_per_frame;
    int max_trace_depth;
    int render_flags;
    int light_count;
    float3 bg_color;

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
    int material_index;
    float3 normal;
    float pad;
};


struct BlendshapeFrame
{
    int delta_offset;
    float weight;
};
struct BlendshapeInfo
{
    int frame_count;
    int frame_offset;
};

struct JointCount
{
    int weight_count;
    int weight_offset;
};
struct JointWeight
{
    float weight;
    int joint_index;
};


