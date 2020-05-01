#include "pch.h"
#include "gptEntity.h"
#include "Foundation/gptUtils.h"

namespace gpt {

int GetTexelSize(Format v)
{
    switch (v) {
    case Format::Ru8: return 1;
    case Format::RGu8: return 2;
    case Format::RGBAu8: return 4;
    case Format::Rf16: return 2;
    case Format::RGf16: return 4;
    case Format::RGBAf16: return 8;
    case Format::Rf32: return 4;
    case Format::RGf32: return 8;
    case Format::RGBAf32: return 16;
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

Texture::Texture(int width, int height, Format format)
{
    m_width = width;
    m_height = height;
    m_format = format;
    markDirty(DirtyFlag::Texture);
}

void Texture::upload(const void* src)
{
    auto v = (const char*)src;
    size_t size = m_width * m_height * GetTexelSize(m_format);
    m_data.assign(v, v + size);
    markDirty(DirtyFlag::TextureData);
}


RenderTarget::RenderTarget(int width, int height, Format format)
{
    m_width = width;
    m_height = height;
    m_format = format;
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

void Material::setRoughnessTexture(ITexture* v)
{
    m_tex_roughness = base_t(v);
    m_data.roughness_tex = GetID(m_tex_roughness);
    markDirty(DirtyFlag::Material);
}

void Material::setEmissiveTexture(ITexture* v)
{
    m_tex_emissive = base_t(v);
    m_data.emissive_tex = GetID(m_tex_emissive);
    markDirty(DirtyFlag::Material);
}

const MaterialData& Material::getData()
{
    return m_data;
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

void Camera::setRenderTarget(IRenderTarget* v)
{
    m_render_target = base_t(v);
    markDirty(DirtyFlag::Camera);
}

RenderTarget* Camera::getRenderTarget()
{
    return m_render_target;
}

const CameraData& Camera::getData()
{
    if (isDirty(DirtyFlag::Camera)) {
        // todo: update matrices
    }
    return m_data;
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

const LightData& Light::getData()
{
    return m_data;
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
    return ret;
}
void Blendshape::setWeight(int frame, float v)
{
    m_frames[frame]->weight = v;
    m_mesh->markDirty(DirtyFlag::Blendshape);
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

int Blendshape::getFrameCount() const
{
    return (int)m_frames.size();
}

void Blendshape::exportDelta(int frame, vertex_t* dst) const
{
    int vc = m_mesh->getVertexCount();
    const Frame& f = *m_frames[frame];
    auto* points    = f.delta_points.empty() ? GetDummyBuffer<float3>(vc) : f.delta_points.cdata();
    auto* normals   = f.delta_normals.empty() ? GetDummyBuffer<float3>(vc) : f.delta_normals.cdata();
    auto* tangents  = f.delta_tangents.empty() ? GetDummyBuffer<float3>(vc) : f.delta_tangents.cdata();
    auto* uv        = f.delta_uv.empty() ? GetDummyBuffer<float2>(vc) : f.delta_uv.cdata();

    vertex_t tmp{};
    for (int vi = 0; vi < vc; ++vi) {
        tmp.point = *points++;
        tmp.normal = *normals++;
        tmp.tangent = *tangents++;
        tmp.uv = *uv++;
        *dst++ = tmp;
    }
}


void Mesh::setIndices(const int* v, size_t n)
{
    if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_indices == MakeIArray(v, n))
        return;

    m_indices.assign(v, v + n);
    m_data.face_count = (uint32_t)m_indices.size() / 3;
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

int Mesh::getFaceCount() const
{
    return (int)m_indices.size() / 3;
}

int Mesh::getIndexCount() const
{
    return (int)m_indices.size();
}

int Mesh::getVertexCount() const
{
    return (int)m_points.size();
}

void Mesh::exportVertices(vertex_t* dst) const
{
    int vc = getVertexCount();
    auto* points    = m_points.cdata();
    auto* normals   = m_normals.empty() ? GetDummyBuffer<float3>(vc) : m_normals.cdata();
    auto* tangents  = m_tangents.empty() ? GetDummyBuffer<float3>(vc) : m_tangents.cdata();
    auto* uv        = m_uv.empty() ? GetDummyBuffer<float2>(vc) : m_uv.cdata();

    vertex_t tmp{};
    for (int vi = 0; vi < vc; ++vi) {
        tmp.point = *points++;
        tmp.normal = *normals++;
        tmp.tangent = *tangents++;
        tmp.uv = *uv++;
        *dst++ = tmp;
    }
}

void Mesh::exportFaces(face_t* dst) const
{
    int fc = getFaceCount();
    auto* indices = m_indices.cdata();
    auto* mids = m_material_ids.empty() ? GetDummyBuffer<int>(fc) : m_material_ids.cdata();

    face_t tmp{};
    for (int fi = 0; fi < fc; ++fi) {
        for (int i = 0; i < 3; ++i)
            tmp.indices[i] = indices[i];
        indices += 3;
        tmp.material_index = *mids++;
        *dst++ = tmp;
    }
}

int Mesh::getJointCount() const
{
    return (int)m_joint_bindposes.size();
}

int Mesh::getJointWeightCount() const
{
    return (int)m_joint_weights.size();
}

const float4x4* Mesh::getJointBindposes() const
{
    return m_joint_bindposes.data();
}

void Mesh::exportJointCounts(JointCount* dst) const
{
    int offset = 0;
    JointCount tmp;
    for (auto c : m_joint_counts) {
        tmp.weight_count = c;
        tmp.weight_offset = offset;
        offset += c;
        *dst++ = tmp;
    }
}

void Mesh::exportJointWeights(JointWeight* dst) const
{
    m_joint_weights.copy_to(dst);
}


int Mesh::getBlendshapeCount() const
{
    return (int)m_blendshapes.size();
}

int Mesh::getBlendshapeFrameCount() const
{
    int r = 0;
    eachBlendshape([&r](auto& bs) {
        r += bs.getFrameCount();
    });
    return r;
}

void Mesh::exportBlendshapes(BlendshapeData* dst) const
{
    int offset = 0;
    BlendshapeData tmp;
    eachBlendshape([&](auto& bs) {
        int n = bs.getFrameCount();
        tmp.frame_count = n;
        tmp.frame_offset = offset;
        offset += n;
        *dst++ = tmp;
    });
}

void Mesh::exportBlendshapeFrames(BlendshapeFrameData* dst) const
{
    int vc = getVertexCount();
    int offset = 0;
    BlendshapeFrameData tmp;
    eachBlendshape([&](auto& bs) {
        bs.eachFrame([&](auto& frame) {
            tmp.delta_offset = offset;
            tmp.weight = frame.weight;
            offset += vc;
            *dst++ = tmp;
        });
    });
}

void Mesh::exportBlendshapeDelta(vertex_t* dst) const
{
    int vc = getVertexCount();
    eachBlendshape([&](auto& bs) {
        int nf = bs.getFrameCount();
        for (int fi = 0; fi < nf; ++fi) {
            bs.exportDelta(fi, dst);
            dst += vc;
        }
    });
}

const MeshData& Mesh::getData()
{
    return m_data;
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
    if (m_mesh->hasJoints()) {
        if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_joint_matrices == MakeIArray(v, m_mesh->getJointCount()))
            return;

        m_joint_matrices.assign(v, v + m_mesh->getJointCount());
        markDirty(DirtyFlag::Joints);
    }
}

void MeshInstance::setBlendshapeWeights(const float* v)
{
    if (m_mesh->hasBlendshapes()) {
        if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_blendshape_weights == MakeIArray(v, m_mesh->getBlendshapeCount()))
            return;

        m_blendshape_weights.assign(v, v + m_mesh->getBlendshapeCount());
        markDirty(DirtyFlag::Blendshape);
    }
}

bool MeshInstance::hasFlag(InstanceFlag v) const
{
    return get_flag(m_instance_flags, v);
}

void MeshInstance::exportJointMatrices(float4x4* dst)
{
    auto& mesh = *m_mesh;
    if (!mesh.hasJoints())
        return;

    int n = mesh.getJointCount();
    if (m_joint_matrices.size() != n)
        m_joint_matrices.resize(n, float4x4::identity());

    // note:
    // object space skinning is recommended for better BLAS building. ( http://intro-to-dxr.cwyman.org/presentations/IntroDXR_RaytracingAPI.pdf )
    // so, try to convert bone matrices to root bone space.
    // on skinned meshes, inst.transform is root bone's transform or identity if root bone is not assigned.
    // both cases work, but identity matrix means world space skinning that is not optimal.

    auto iroot = mu::invert(m_data.local_to_world);
    auto* bindposes = mesh.getJointBindposes();
    for (int ji = 0; ji < n; ++ji)
        *dst++ = bindposes[ji] * m_joint_matrices[ji] * iroot;
}

void MeshInstance::exportBlendshapeWeights(float* dst)
{
    auto& mesh = *m_mesh;
    if (!mesh.hasBlendshapes())
        return;

    int n = mesh.getBlendshapeCount();
    if (m_blendshape_weights.size() != n)
        m_blendshape_weights.resize(n, 0.0f);

    m_blendshape_weights.copy_to(dst);
}

Mesh* MeshInstance::getMesh()
{
    return m_mesh;
}

Material* MeshInstance::getMaterial()
{
    return m_material;
}

const InstanceData& MeshInstance::getData()
{
    return m_data;
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
    m_camera.reset();
    m_lights.clear();
    m_instances.clear();
    markDirty(DirtyFlag::SceneEntities);
}

Camera* Scene::getCamera()
{
    return m_camera;
}

const SceneData& Scene::getData()
{
    m_data.samples_per_frame = Globals::getInstance().getSamplesPerFrame();
    m_data.max_trace_depth = Globals::getInstance().getMaxTraceDepth();

    if (m_camera)
        m_data.camera = m_camera->getData();

    int nlights = std::min((int)m_lights.size(), gptMaxLights);
    m_data.light_count = nlights;
    for (int li = 0; li < nlights; ++li)
        m_data.lights[li] = m_lights[li]->getData();

    return m_data;
}

void Scene::incrementFrameCount()
{
    m_data.frame++;
}

} // namespace gpt 

gptAPI gpt::IGlobals* gptGetGlobals()
{
    return &gpt::Globals::getInstance();
}

gpt::IContext* gptCreateContextCPU();
gpt::IContext* gptCreateContextDXR();

gptAPI gpt::IContext* gptCreateContext_(gpt::DeviceType type)
{
    switch (type) {
    //case gpt::DeviceType::CPU: return gptCreateContextCPU();
    case gpt::DeviceType::DXR: return gptCreateContextDXR();
    default: return nullptr;
    }
}
