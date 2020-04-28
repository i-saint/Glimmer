#include "pch.h"
#include "lptEntity.h"
#include "Foundation//lptUtils.h"

namespace lpt {

int GetTexelSize(TextureFormat v)
{
    switch (v) {
    case TextureFormat::RGBAu8: return 4;
    case TextureFormat::RGBAf16: return 8;
    case TextureFormat::RGBAf32: return 16;
    default: return 0;
    }
}

Globals& Globals::getInstance()
{
    static Globals s_inst;
    return s_inst;
}

Globals::Globals()
{

}

Globals::~Globals()
{

}

void Globals::setFlag(GlobalFlag f, bool v)
{
    set_flag(m_flags, f, v);
}

bool Globals::getFlag(GlobalFlag f) const
{
    return get_flag(m_flags, f);
}

void Globals::enableStrictUpdateCheck(bool v)
{
    setFlag(GlobalFlag::StrictUpdateCheck, v);
}
void Globals::enablePowerStableState(bool v)
{
    setFlag(GlobalFlag::PowerStableState, v);
}
void Globals::enableTimestamp(bool v)
{
    setFlag(GlobalFlag::Timestamp, v);
}
void Globals::enableForceUpdateAS(bool v)
{
    setFlag(GlobalFlag::ForceUpdateAS, v);
}
void Globals::setSamplesPerFrame(int v)
{
    m_samples_per_frame = v;
}
void Globals::setMaxTraceDepth(int v)
{
    m_max_trace_depth = v;
}

bool Globals::isStrictUpdateCheckEnabled() const
{
    return getFlag(GlobalFlag::StrictUpdateCheck);
}
bool Globals::isPowerStableStateEnabled() const
{
    return getFlag(GlobalFlag::PowerStableState);
}
bool Globals::isTimestampEnabled() const
{
    return getFlag(GlobalFlag::Timestamp);
}
bool Globals::isForceUpdateASEnabled() const
{
    return getFlag(GlobalFlag::ForceUpdateAS);
}
int Globals::getSamplesPerFrame() const
{
    return m_samples_per_frame;
}
int Globals::getMaxTraceDepth() const
{
    return m_max_trace_depth;
}

Texture::Texture(TextureFormat format, int width, int height)
{
    m_format = format;
    m_width = width;
    m_height = height;
    markDirty(DirtyFlag::Texture);
}

void Texture::upload(const void* src)
{
    auto v = (const char*)src;
    size_t size = m_width * m_height * GetTexelSize(m_format);
    m_data.assign(v, v + size);
    markDirty(DirtyFlag::TextureData);
}


RenderTarget::RenderTarget(TextureFormat format, int width, int height)
{
    m_format = format;
    m_width = width;
    m_height = height;
    markDirty(DirtyFlag::Texture);
}

void RenderTarget::enableReadback(bool v)
{
    m_readback_enabled = v;
}


Material::Material()
{
    markDirty(DirtyFlag::Material);
}

void Material::setType(MaterialType v)
{
    m_data.type = v;
    markDirty(DirtyFlag::Material);
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
    m_tex_diffuse = base_t(v);
    m_data.diffuse_tex = GetID(m_tex_diffuse);
    markDirty(DirtyFlag::Material);
}

void Material::setEmissiveTexture(ITexture* v)
{
    m_tex_emissive = base_t(v);
    m_data.emissive_tex = GetID(m_tex_emissive);
    markDirty(DirtyFlag::Material);
}


Camera::Camera()
{
    markDirty(DirtyFlag::Camera);
}

void Camera::setPosition(float3 v)
{
    m_data.position = v;
    markDirty(DirtyFlag::Camera);
}
void Camera::setDirection(float3 v, float3 up)
{
    m_data.rotation = mu::look_quat(v, up);
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


Light::Light()
{
    markDirty(DirtyFlag::Light);
}

void Light::setType(LightType v)
{
    m_data.type = v;
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


Blendshape::Blendshape(Mesh* mesh)
    : m_mesh(mesh)
{
}
void Blendshape::setName(const char* name)
{
    m_name = name;
}
int Blendshape::addFrame()
{
    int ret = (int)m_frames.size();
    m_frames.push_back(std::make_unique<Frame>());
    m_mesh->markDirty(DirtyFlag::Blendshape);
    return ret;
}
void Blendshape::setDeltaPoints(int frame, const float3* v, size_t n)
{
    m_frames[frame]->delta_points.assign(v, n);
    m_mesh->markDirty(DirtyFlag::Blendshape);
}
void Blendshape::setDeltaNormals(int frame, const float3* v, size_t n)
{
    m_frames[frame]->delta_normals.assign(v, n);
    m_mesh->markDirty(DirtyFlag::Blendshape);
}
void Blendshape::setDeltaTangents(int frame, const float3* v, size_t n)
{
    m_frames[frame]->delta_tangents.assign(v, n);
    m_mesh->markDirty(DirtyFlag::Blendshape);
}
void Blendshape::setDeltaUV(int frame, const float2* v, size_t n)
{
    m_frames[frame]->delta_uv.assign(v, n);
    m_mesh->markDirty(DirtyFlag::Blendshape);
}


void Mesh::setIndices(const int* v, size_t n)
{
    if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_indices == MakeIArray(v, n))
        return;

    m_indices.assign(v, v + n);
    m_data.face_count = (uint32_t)m_indices.size() / 3;
    m_data.index_count = (uint32_t)m_indices.size();
    markDirty(DirtyFlag::Indices);
}

void Mesh::setPoints(const float3* v, size_t n)
{
    if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_points == MakeIArray(v, n))
        return;

    m_points.assign(v, v + n);
    m_data.vertex_count = (uint32_t)m_points.size();
    markDirty(DirtyFlag::Points);
}

void Mesh::setNormals(const float3* v, size_t n)
{
    if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_normals == MakeIArray(v, n))
        return;

    m_normals.assign(v, v + n);
    markDirty(DirtyFlag::Normals);
}

void Mesh::setTangents(const float3* v, size_t n)
{
    if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_tangents == MakeIArray(v, n))
        return;

    m_tangents.assign(v, v + n);
    markDirty(DirtyFlag::Tangents);
}

void Mesh::setUV(const float2* v, size_t n)
{
    if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_uv == MakeIArray(v, n))
        return;

    m_uv.assign(v, v + n);
    markDirty(DirtyFlag::UV);
}

void Mesh::setMaterialIDs(const int* v, size_t n)
{
    if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_material_ids == MakeIArray(v, n))
        return;

    m_material_ids.assign(v, v + n);
    markDirty(DirtyFlag::Material);
}

void Mesh::setJointBindposes(const float4x4* v, size_t n)
{
    if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_joint_bindposes == MakeIArray(v, n))
        return;

    m_joint_bindposes.assign(v, v + n);
    markDirty(DirtyFlag::Joints);
    set_flag(m_data.flags, MeshFlag::HasJoints, true);
}

void Mesh::setJointWeights(const JointWeight* v, size_t n)
{
    if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_joint_weights == MakeIArray(v, n))
        return;

    m_joint_weights.assign(v, v + n);
    markDirty(DirtyFlag::Joints);
    set_flag(m_data.flags, MeshFlag::HasJoints, true);
}

void Mesh::setJointCounts(const int* v, size_t n)
{
    if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_joint_counts == MakeIArray(v, n))
        return;

    m_joint_counts.assign(v, v + n);
    markDirty(DirtyFlag::Joints);
    set_flag(m_data.flags, MeshFlag::HasJoints, true);
}

void Mesh::clearJoints()
{
    m_joint_bindposes.clear();
    m_joint_weights.clear();
    m_joint_counts.clear();
    set_flag(m_data.flags, MeshFlag::HasJoints, false);
}

IBlendshape* Mesh::addBlendshape()
{
    auto ret = std::make_shared<Blendshape>(this);
    m_blendshapes.push_back(ret);
    markDirty(DirtyFlag::Blendshape);
    set_flag(m_data.flags, MeshFlag::HasBlendshapes, true);
    return ret.get();
}

void Mesh::clearBlendshapes()
{
    m_blendshapes.clear();
    markDirty(DirtyFlag::Blendshape);
    set_flag(m_data.flags, MeshFlag::HasBlendshapes, false);
}

void Mesh::markDynamic()
{
    set_flag(m_data.flags, MeshFlag::IsDynamic, true);
}

void Mesh::updateFaceData()
{
    if (isDirty(DirtyFlag::Shape)) {
        mu::GenerateTriangleFaceNormals(m_face_normals, m_points, m_indices, false);
    }
}

bool Mesh::hasBlendshapes() const
{
    return !m_blendshapes.empty();
}

bool Mesh::hasJoints() const
{
    return m_joint_counts.size() == getVertexCount() && !m_joint_bindposes.empty() && !m_joint_weights.empty();
}

bool Mesh::isDynamic() const
{
    return (m_data.flags & (int)MeshFlag::IsDynamic);
}

size_t Mesh::getFaceCount() const
{
    return m_indices.size() / 3;
}

size_t Mesh::getIndexCount() const
{
    return m_indices.size();
}

size_t Mesh::getVertexCount() const
{
    return m_points.size();
}


static RawVector<char> GetDummyBuffer_()
{
    static RawVector<char> s_buffer;
    return s_buffer;
}
template<class T>
inline T* GetDummyBuffer(size_t n)
{
    auto& buf = GetDummyBuffer_();
    size_t size = sizeof(T) * n;
    if (buf.size() < size)
        buf.resize_zeroclear(size);
    return (T*)buf.data();
}

void Mesh::exportVertices(vertex_t* dst)
{
    size_t vc = getVertexCount();
    auto* points    = m_points.cdata();
    auto* normals   = m_normals.empty() ? GetDummyBuffer<float3>(vc) : m_normals.cdata();
    auto* tangents  = m_tangents.empty() ? GetDummyBuffer<float3>(vc) : m_tangents.cdata();
    auto* uv        = m_uv.empty() ? GetDummyBuffer<float2>(vc) : m_uv.cdata();

    vertex_t tmp{};
    for (size_t vi = 0; vi < vc; ++vi) {
        tmp.point = *points++;
        tmp.normal = *normals++;
        tmp.tangent = *tangents++;
        tmp.uv = *uv++;
        *dst++ = tmp;
    }
}

void Mesh::exportFaces(face_t* dst)
{
    updateFaceData();

    size_t fc = getFaceCount();
    auto* indices = m_indices.cdata();
    auto* normals = m_face_normals.cdata();
    auto* mids = m_material_ids.empty() ? GetDummyBuffer<int>(fc) : m_material_ids.cdata();

    face_t tmp{};
    for (size_t fi = 0; fi < fc; ++fi) {
        for (int i = 0; i < 3; ++i)
            tmp.indices[i] = indices[i];
        indices += 3;
        tmp.material_index = *mids++;
        tmp.normal = *normals++;
        *dst++ = tmp;
    }
}



MeshInstance::MeshInstance(IMesh* v)
{
    m_mesh = base_t(v);
    m_data.mesh_id = GetID(m_mesh);
    markDirty(DirtyFlag::Mesh);
}

void MeshInstance::setMaterial(IMaterial* v, int slot)
{
    if (slot >= _countof(m_data.material_ids)) {
        mu::DbgBreak();
        return;
    }

    m_material = base_t(v);
    m_data.material_ids[slot] = GetID(m_material);
    markDirty(DirtyFlag::Material);
}

void MeshInstance::setTransform(const float4x4& v)
{
    m_data.local_to_world = v;
    m_data.world_to_local = mu::invert(v);
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


void Scene::setEnabled(bool v)
{
    m_enabled = v;
    markDirty(DirtyFlag::Scene);
}

void Scene::setBackgroundColor(float3 v)
{
    m_data.bg_color = v;
    markDirty(DirtyFlag::Scene);
}

void Scene::setRenderTarget(IRenderTarget* v)
{
    m_render_target = base_t(v);
    markDirty(DirtyFlag::RenderTarget);
}

void Scene::setCamera(ICamera* v)
{
    m_camera = base_t(v);
    markDirty(DirtyFlag::Camera);
}

void Scene::addLight(ILight* v)
{
    m_lights.push_back(base_t(v));
    markDirty(DirtyFlag::Light);
}

void Scene::removeLight(ILight* v)
{
    if (erase(m_lights, v))
        markDirty(DirtyFlag::Light);
}

void Scene::addMesh(IMeshInstance* v)
{
    m_instances.push_back(base_t(v));
    markDirty(DirtyFlag::Instance);
}

void Scene::removeMesh(IMeshInstance* v)
{
    if (erase(m_instances, v))
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

void Scene::updateData()
{
    m_data.frame++;
    m_data.samples_per_frame = Globals::getInstance().getSamplesPerFrame();
    m_data.max_trace_depth = Globals::getInstance().getMaxTraceDepth();

    if (m_camera)
        m_data.camera = m_camera->m_data;

    int nlights = std::min((int)m_lights.size(), lptMaxLights);
    m_data.light_count = nlights;
    for (int li = 0; li < nlights; ++li)
        m_data.lights[li] = m_lights[li]->m_data;
}

} // namespace lpt 

lptAPI lpt::IGlobals* lptGetGlobals()
{
    return &lpt::Globals::getInstance();
}
