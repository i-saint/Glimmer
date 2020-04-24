#include "pch.h"
#include "lptEntity.h"

namespace lpt {

int SizeOfTexel(TextureFormat v)
{
    switch (v) {
    case TextureFormat::Ru8: return 1;
    case TextureFormat::RGu8: return 2;
    case TextureFormat::RGBAu8: return 4;
    case TextureFormat::Rf16: return 2;
    case TextureFormat::RGf16: return 4;
    case TextureFormat::RGBAf16: return 8;
    case TextureFormat::Rf32: return 4;
    case TextureFormat::RGf32: return 8;
    case TextureFormat::RGBAf32: return 16;
    default: return 0;
    }
}

void GlobalSettings::enableDebugFlag(DebugFlag v)
{
    debug_flags = debug_flags | (uint32_t)v;
}

void GlobalSettings::disableDebugFlag(DebugFlag v)
{
    debug_flags = debug_flags & (~(uint32_t)v);
}

bool GlobalSettings::hasDebugFlag(DebugFlag v) const
{
    return (debug_flags & (uint32_t)v) != 0;
}

bool GlobalSettings::hasFlag(GlobalFlag v) const
{
    return (flags & (uint32_t)v) != 0;
}

GlobalSettings& GetGlobals()
{
    static GlobalSettings s_globals;
    return s_globals;
}


void Camera::setPosition(float3 v)
{
    m_data.position = v;
    markDirty(DirtyFlag::Camera);
}
void Camera::setRotation(quatf v)
{
    m_data.rotation = v;
    markDirty(DirtyFlag::Camera);
}
void Camera::setFOV(float v)
{
    m_data.fov = v;
    markDirty(DirtyFlag::Camera);
}
void Camera::setNear(float v)
{
    m_data.near_plane = v;
    markDirty(DirtyFlag::Camera);
}

void Camera::setFar(float v)
{
    m_data.far_plane = v;
    markDirty(DirtyFlag::Camera);
}


void Light::setType(LightType v)
{
    m_data.light_type = v;
    markDirty(DirtyFlag::Light);
}

void Light::setPosition(float3 v)
{
    m_data.position = v;
    markDirty(DirtyFlag::Light);
}

void Light::setDirection(float3 v)
{
    m_data.direction = v;
    markDirty(DirtyFlag::Light);
}

void Light::setRange(float v)
{
    m_data.range = v;
    markDirty(DirtyFlag::Light);
}

void Light::setSpotAngle(float v)
{
    m_data.spot_angle = v;
    markDirty(DirtyFlag::Light);
}

void Light::setColor(float3 v)
{
    m_data.color = v;
    markDirty(DirtyFlag::Light);
}


void Texture::setup(TextureFormat format, int width, int height)
{
    m_format = format;
    m_width = width;
    m_height = height;
    markDirty(DirtyFlag::Texture);
}

void Texture::upload(const void* src)
{
    auto v = (const char*)src;
    size_t size = m_width * m_height * SizeOfTexel(m_format);
    m_data.assign(v, v + size);
    markDirty(DirtyFlag::TextureData);
}


void RenderTarget::setup(TextureFormat format, int width, int height)
{
    m_format = format;
    m_width = width;
    m_height = height;
    markDirty(DirtyFlag::Texture);
}

void RenderTarget::readback(void* dst)
{
    m_readback_request = dst;
}


void Material::setDiffuse(float3 v)
{
    m_data.diffuse = v;
    markDirty(DirtyFlag::Material);
}

void Material::setRoughness(float v)
{
    m_data.roughness = v;
    markDirty(DirtyFlag::Material);
}

void Material::setEmissive(float3 v)
{
    m_data.emissive = v;
    markDirty(DirtyFlag::Material);
}

void Material::setDiffuseTexture(ITexture* v)
{
    m_data.diffuse_tex = v ? static_cast<Texture*>(v)->m_index : 0;
    markDirty(DirtyFlag::Material);
}

void Material::setEmissionTexture(ITexture* v)
{
    m_data.emissive_tex = v ? static_cast<Texture*>(v)->m_index : 0;
    markDirty(DirtyFlag::Material);
}


void Mesh::setIndices(const int* v, size_t n)
{
    m_indices.assign(v, v + n);
    markDirty(DirtyFlag::Indices);
}

void Mesh::setPoints(const float3* v, size_t n)
{
    m_points.assign(v, v + n);
    markDirty(DirtyFlag::Points);
}

void Mesh::setNormals(const float3* v, size_t n)
{
    m_normals.assign(v, v + n);
    markDirty(DirtyFlag::Normals);
}

void Mesh::setTangents(const float3* v, size_t n)
{
    m_tangents.assign(v, v + n);
    markDirty(DirtyFlag::Tangents);
}

void Mesh::setUV(const float2* v, size_t n)
{
    m_uv.assign(v, v + n);
    markDirty(DirtyFlag::UV);
}

void Mesh::setJointBindposes(const float4x4* v, size_t n)
{
    m_joint_bindposes.assign(v, v + n);
    markDirty(DirtyFlag::Joints);
}

void Mesh::setJointWeights(const JointWeight* v, size_t n)
{
    m_joint_weights.assign(v, v + n);
    markDirty(DirtyFlag::Joints);
}

void Mesh::setJointCounts(const uint8_t* v, size_t n)
{
    m_joint_counts.assign(v, v + n);
    markDirty(DirtyFlag::Joints);
}


void MeshInstance::setMesh(IMesh* v)
{
    m_mesh = dynamic_cast<Mesh*>(v);
    markDirty(DirtyFlag::Mesh);
}

void MeshInstance::setMaterial(IMaterial* v)
{
    m_material = dynamic_cast<Material*>(v);
    markDirty(DirtyFlag::Material);
}

void MeshInstance::setTransform(const float4x4& v)
{
    m_transform = v;
    markDirty(DirtyFlag::Transform);
}

void MeshInstance::setJointMatrices(const float4x4* v)
{
    if (m_mesh && !m_mesh->m_joint_bindposes.empty()) {
        m_joint_matrices.assign(v, v + m_mesh->m_joint_bindposes.size());
        markDirty(DirtyFlag::Joints);
    }
}

bool MeshInstance::hasFlag(InstanceFlag v) const
{
    return (m_instance_flags & (uint32_t)v) != 0;
}


void Scene::setRenderTarget(IRenderTarget* v)
{
    m_render_target = static_cast<RenderTarget*>(v);
    markDirty(DirtyFlag::RenderTarget);
}

void Scene::setCamera(ICamera* v)
{
    m_camera = static_cast<Camera*>(v);
    markDirty(DirtyFlag::Camera);
}

void Scene::addLight(ILight* v)
{
    m_lights.push_back(static_cast<Light*>(v));
    markDirty(DirtyFlag::Light);
}

void Scene::addMesh(IMeshInstance* v)
{
    m_instances.push_back(static_cast<MeshInstance*>(v));
    markDirty(DirtyFlag::Instance);
}

void Scene::clear()
{
    m_render_target.reset();
    m_camera.reset();
    m_lights.clear();
    m_instances.clear();
    markDirty(DirtyFlag::SceneEntities);
}

} // namespace lpt 
