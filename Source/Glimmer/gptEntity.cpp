#include "pch.h"
#include "gptEntity.h"
#include "gptWindow.h"
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
    enableGenerateTangents(true);
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

void Globals::enableGenerateTangents(bool v)
{
    setFlag(GlobalFlag::GenerateTangents, v);
}
void Globals::enableStrictUpdateCheck(bool v)
{
    setFlag(GlobalFlag::StrictUpdateCheck, v);
}
void Globals::enableTimestamp(bool v)
{
    setFlag(GlobalFlag::Timestamp, v);
}
void Globals::enablePowerStableState(bool v)
{
    setFlag(GlobalFlag::PowerStableState, v);
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

bool Globals::isGenerateTangentsEnabled() const
{
    return getFlag(GlobalFlag::GenerateTangents);
}
bool Globals::isStrictUpdateCheckEnabled() const
{
    return getFlag(GlobalFlag::StrictUpdateCheck);
}
bool Globals::isTimestampEnabled() const
{
    return getFlag(GlobalFlag::Timestamp);
}
bool Globals::isPowerStableStateEnabled() const
{
    return getFlag(GlobalFlag::PowerStableState);
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
    m_data.resize_zeroclear(m_width * m_height * GetTexelSize(m_format));
    markDirty(DirtyFlag::Texture);
}

void Texture::upload(const void* src)
{
    m_data.assign((const char*)src, m_data.size());
    markDirty(DirtyFlag::TextureData);
}

int Texture::getWidth() const { return m_width; }
int Texture::getHeight() const { return m_height; }
Format Texture::getFormat() const { return m_format; }
Span<char> Texture::getData() const { return m_data; }


RenderTarget::RenderTarget(int width, int height, Format format)
{
    m_width = width;
    m_height = height;
    m_format = format;
    markDirty(DirtyFlag::Texture);
}

RenderTarget::RenderTarget(IWindow* window, Format format)
    : RenderTarget(window->getWidth(), window->getHeight(), format)
{
    m_window = base_t(window);
}

void RenderTarget::enableReadback(bool v)
{
    m_readback_enabled = v;
}

int RenderTarget::getWidth() const { return m_width; }
int RenderTarget::getHeight() const { return m_height; }
Format RenderTarget::getFormat() const { return m_format; }
IWindow* RenderTarget::getWindow() const { return m_window; }


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
    m_data.roughness = mu::clamp01(v);
    markDirty(DirtyFlag::Material);
}

void Material::setEmissive(float3 v)
{
    m_data.emissive = v;
    markDirty(DirtyFlag::Material);
}

void Material::setEmissiveRange(float v)
{
    m_data.emissive_range = v;
    markDirty(DirtyFlag::Material);
}

void Material::setRefractionIndex(float v)
{
    m_data.refraction_index = v;
    markDirty(DirtyFlag::Material);
}

void Material::setOpacity(float v)
{
    m_data.opacity = v;
    markDirty(DirtyFlag::Material);
}

void Material::setEmissiveSampleCount(int v)
{
    m_data.emissive_sample_count = v;
    markDirty(DirtyFlag::Material);
}

void Material::setDiffuseMap(ITexture* v)
{
    m_tex_diffuse = base_t(v);
    m_data.diffuse_tex = GetID(m_tex_diffuse);
    markDirty(DirtyFlag::Material);
}

void Material::setOpacityMap(ITexture* v)
{
    m_tex_opacity = base_t(v);
    m_data.opacity_tex = GetID(m_tex_opacity);
    markDirty(DirtyFlag::Material);
}

void Material::setRoughnessMap(ITexture* v)
{
    m_tex_roughness = base_t(v);
    m_data.roughness_tex = GetID(m_tex_roughness);
    markDirty(DirtyFlag::Material);
}

void Material::setEmissiveMap(ITexture* v)
{
    m_tex_emissive = base_t(v);
    m_data.emissive_tex = GetID(m_tex_emissive);
    markDirty(DirtyFlag::Material);
}

void Material::setNormalMap(ITexture* v)
{
    m_tex_normal = base_t(v);
    m_data.normal_tex = GetID(m_tex_normal);
    markDirty(DirtyFlag::Material);
}

MaterialType Material::getType() const { return m_data.type; }
float3 Material::getDiffuse() const { return m_data.diffuse; }
float Material::getRoughness() const { return m_data.roughness; }
float3 Material::getEmissive() const { return m_data.emissive; }
float Material::getEmissiveRange() const { return m_data.emissive_range; }
int Material::getEmissiveSampleCount() const { return m_data.emissive_sample_count; }
float Material::getRefractionIndex() const { return m_data.refraction_index; }
float Material::getOpacity() const { return m_data.opacity; }
ITexture* Material::getDiffuseMap() const { return m_tex_diffuse; }
ITexture* Material::getOpacityMap() const { return m_tex_opacity; }
ITexture* Material::getRoughnessMap() const { return m_tex_roughness; }
ITexture* Material::getEmissiveMap() const { return m_tex_emissive; }
ITexture* Material::getNormalMap() const { return m_tex_normal; }

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
void Camera::setDirection(float3 dir, float3 up)
{
    m_data.rotation = mu::look_quat(dir, up);
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

float3 Camera::getPosition() const { return m_data.position; }
float3 Camera::getDirection() const { return to_mat3x3(m_data.rotation) * float3 {0.0f, 0.0f, 1.0f}; }
float Camera::getFOV() const { return m_data.fov; }
float Camera::getNear() const { return m_data.near_plane; }
float Camera::getFar() const { return m_data.far_plane; }
IRenderTarget* Camera::getRenderTarget() const { return m_render_target; }

const CameraData& Camera::getData()
{
    if (isDirty() || (m_render_target && m_render_target->isDirty())) {
        float aspect = 1.0f;
        if (m_render_target)
            aspect = (float)m_render_target->getWidth() / (float)m_render_target->getHeight();
        m_data.proj = mu::perspective(m_data.fov, aspect, m_data.near_plane, m_data.far_plane);
        m_data.view = mu::invert(mu::transform(m_data.position, m_data.rotation));
        m_data.viewproj = m_data.view * m_data.proj;
    }
    return m_data;
}


Light::Light()
{
    markDirty(DirtyFlag::Light);
}

void Light::setEnabled(bool v)
{
    m_enabled = v;
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

void Light::setIntensity(float v)
{
    m_data.intensity = v;
    markDirty(DirtyFlag::Light);
}

void Light::setDisperse(float v)
{
    m_data.disperse = v;
    markDirty(DirtyFlag::Light);
}

bool Light::isEnabled() const { return m_enabled; }
LightType Light::getType() const { return m_data.type; }
float3 Light::getPosition() const { return m_data.position; }
float3 Light::getDirection() const { return m_data.direction; }
float Light::getRange() const { return m_data.range; }
float Light::getSpotAngle() const { return m_data.spot_angle; }
float3 Light::getColor() const { return m_data.color; }
float Light::getIntensity() const { return m_data.intensity; }
float Light::getDisperse() const { return m_data.disperse; }

const LightData& Light::getData()
{
    return m_data;
}


BlendshapeFrame::BlendshapeFrame(float w)
    : m_weight(w)
{
}

void BlendshapeFrame::setDeltaPoints(const float3* v, size_t n)
{
    m_delta_points.assign(v, n);
}
void BlendshapeFrame::setDeltaNormals(const float3* v, size_t n)
{
    m_delta_normals.assign(v, n);
}
void BlendshapeFrame::setDeltaTangents(const float3* v, size_t n)
{
    m_delta_tangents.assign(v, n);
}
void BlendshapeFrame::setDeltaUV(const float2* v, size_t n)
{
    m_delta_uv.assign(v, n);
}
Span<float3> BlendshapeFrame::getDeltaPoints() const { return m_delta_points; }
Span<float3> BlendshapeFrame::getDeltaNormals() const { return m_delta_normals; }
Span<float3> BlendshapeFrame::getDeltaTangents() const { return m_delta_tangents; }
Span<float2> BlendshapeFrame::getDeltaUV() const { return m_delta_uv; }



Blendshape::Blendshape(Mesh* mesh)
    : m_mesh(mesh)
{
}
void Blendshape::setName(const char* name)
{
    m_name = name;
}

IBlendshapeFrame* Blendshape::getFrame(int i)
{
    if (i < 0 || i >= m_frames.size())
        return nullptr;
    return m_frames[i].get();
}

IBlendshapeFrame* Blendshape::addFrame(float weight)
{
    auto ret = std::make_shared<BlendshapeFrame>(weight);
    m_frames.push_back(ret);
    std::sort(m_frames.begin(), m_frames.end(),
        [](auto& a, auto& b) { return a->m_weight < b->m_weight; });
    return ret.get();
}

void Blendshape::removeFrame(IBlendshapeFrame* f)
{
    erase_if(m_frames, [f](auto& p) { return p.get() == f; });
}

int Blendshape::getFrameCount() const
{
    return (int)m_frames.size();
}

void Blendshape::exportDelta(int frame, vertex_t* dst) const
{
    int vc = m_mesh->getVertexCount();
    auto& f = *m_frames[frame];
    auto* points    = f.m_delta_points.empty() ? GetDummyBuffer<float3>(vc) : f.m_delta_points.cdata();
    auto* normals   = f.m_delta_normals.empty() ? GetDummyBuffer<float3>(vc) : f.m_delta_normals.cdata();
    auto* tangents  = f.m_delta_tangents.empty() ? GetDummyBuffer<float3>(vc) : f.m_delta_tangents.cdata();
    auto* uv        = f.m_delta_uv.empty() ? GetDummyBuffer<float2>(vc) : f.m_delta_uv.cdata();

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
    if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_indices == MakeSpan(v, n))
        return;

    m_indices.assign(v, v + n);
    m_data.face_count = (uint32_t)m_indices.size() / 3;
    markDirty(DirtyFlag::Indices);
}

void Mesh::setPoints(const float3* v, size_t n)
{
    if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_points == MakeSpan(v, n))
        return;

    m_points.assign(v, v + n);
    m_data.vertex_count = (uint32_t)m_points.size();
    markDirty(DirtyFlag::Points);
}

void Mesh::setNormals(const float3* v, size_t n)
{
    if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_normals == MakeSpan(v, n))
        return;

    m_normals.assign(v, v + n);
    markDirty(DirtyFlag::Normals);
}

void Mesh::setTangents(const float3* v, size_t n)
{
    if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_tangents == MakeSpan(v, n))
        return;

    m_tangents.assign(v, v + n);
    markDirty(DirtyFlag::Tangents);
}

void Mesh::setUV(const float2* v, size_t n)
{
    if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_uv == MakeSpan(v, n))
        return;

    m_uv.assign(v, v + n);
    markDirty(DirtyFlag::UV);
}

void Mesh::setMaterialIDs(const int* v, size_t n)
{
    if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_material_ids == MakeSpan(v, n))
        return;

    m_material_ids.assign(v, v + n);
    markDirty(DirtyFlag::Material);
}

Span<int>    Mesh::getIndices() const { return m_indices; }
Span<float3> Mesh::getPoints() const { return m_points; }
Span<float3> Mesh::getNormals() const { return m_normals; }
Span<float3> Mesh::getTangents() const { return m_tangents; }
Span<float2> Mesh::getUV() const { return m_uv; }
Span<int>    Mesh::getMaterialIDs() const { return m_material_ids; }

void Mesh::markDynamic()
{
    set_flag(m_data.flags, MeshFlag::IsDynamic, true);
}


void Mesh::setJointBindposes(const float4x4* v, size_t n)
{
    if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_joint_bindposes == MakeSpan(v, n))
        return;

    m_joint_bindposes.assign(v, v + n);
    markDirty(DirtyFlag::Joints);
    set_flag(m_data.flags, MeshFlag::HasJoints, true);
}

void Mesh::setJointWeights(const JointWeight* v, size_t n)
{
    if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_joint_weights == MakeSpan(v, n))
        return;

    m_joint_weights.assign(v, v + n);
    markDirty(DirtyFlag::Joints);
    set_flag(m_data.flags, MeshFlag::HasJoints, true);
}

void Mesh::setJointCounts(const int* v, size_t n)
{
    if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_joint_counts == MakeSpan(v, n))
        return;

    m_joint_counts.assign(v, v + n);
    markDirty(DirtyFlag::Joints);
    set_flag(m_data.flags, MeshFlag::HasJoints, true);
}

Span<float4x4>    Mesh::getJointBindposes() const { return m_joint_bindposes; }
Span<JointWeight> Mesh::getJointWeights() const { return m_joint_weights; }
Span<int>         Mesh::getJointCounts() const { return m_joint_counts; }

int Mesh::getBlendshapeCount() const
{
    return (int)m_blendshapes.size();
}

IBlendshape* Mesh::getBlendshape(int i)
{
    if (i < 0 || i >= m_blendshapes.size())
        return nullptr;
    return m_blendshapes[i].get();
}

IBlendshape* Mesh::addBlendshape()
{
    auto ret = std::make_shared<Blendshape>(this);
    m_blendshapes.push_back(ret);
    markDirty(DirtyFlag::Blendshape);
    set_flag(m_data.flags, MeshFlag::HasBlendshapes, true);
    return ret.get();
}

void Mesh::removeBlendshape(IBlendshape* f)
{
    if (erase_if(m_blendshapes, [f](auto& p) { return p.get() == f; })) {
        if (m_blendshapes.empty())
            set_flag(m_data.flags, MeshFlag::HasBlendshapes, false);
        markDirty(DirtyFlag::Blendshape);
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
    if (m_normals.size() != m_points.size()) {
        auto& normals = const_cast<RawVector<float3>&>(m_normals); // ...
        normals.resize_discard(getVertexCount());
        mu::GenerateNormalsTriangleIndexed(normals.data(), m_points.data(), m_indices.data(), getFaceCount(), getVertexCount());
    }
    if (Globals::getInstance().isGenerateTangentsEnabled() && m_uv.size() == m_points.size() && m_tangents.size() != m_points.size()) {
        auto& tangents = const_cast<RawVector<float3>&>(m_tangents); // ...
        tangents.resize_discard(getVertexCount());
        mu::GenerateTangentsTriangleIndexed(tangents.data(), m_points.data(), m_uv.data(), m_normals.data(), m_indices.data(), getFaceCount(), getVertexCount());
    }

    int vc = getVertexCount();
    auto* points    = m_points.cdata();
    auto* normals   = m_normals.cdata();
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
    auto* indices = (const int3*)m_indices.cdata();
    auto* mids = m_material_ids.empty() ? GetDummyBuffer<int>(fc) : m_material_ids.cdata();

    face_t tmp{};
    for (int fi = 0; fi < fc; ++fi) {
        tmp.indices = *indices++;
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
            tmp.weight = frame.m_weight;
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
    if (isDirty(DirtyFlag::Points)) {
        mu::MinMax(m_points.data(), m_points.size(), m_data.bb_min, m_data.bb_max);
    }
    return m_data;
}



MeshInstance::MeshInstance(IMesh* v)
{
    m_mesh = base_t(v);
    m_data.mesh_id = GetID(m_mesh);
    m_materials.resize(_countof(m_data.material_ids));
    markDirty(DirtyFlag::Mesh);
}

void MeshInstance::setEnabled(bool v)
{
    m_enabled = v;
    markDirty(DirtyFlag::Transform);
}

void MeshInstance::setFlag(InstanceFlag f, bool v)
{
    set_flag(m_data.instance_flags, f, v);
    markDirty(DirtyFlag::Flags);
}

void MeshInstance::setMaterial(IMaterial* v, int slot)
{
    if (slot < 0 || slot >= _countof(m_data.material_ids)) {
        mu::DbgBreak();
        return;
    }

    m_materials[slot] = base_t(v);
    m_data.material_ids[slot] = GetID(v);
    markDirty(DirtyFlag::Material);
}

void MeshInstance::setTransform(const float4x4& v)
{
    m_data.transform = v;
    m_data.itransform = mu::invert(v);
    markDirty(DirtyFlag::Transform);
}

void MeshInstance::setJointMatrices(const float4x4* v)
{
    if (m_mesh->hasJoints()) {
        if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_joint_matrices == MakeSpan(v, m_mesh->getJointCount()))
            return;

        m_joint_matrices.assign(v, v + m_mesh->getJointCount());
        markDirty(DirtyFlag::Joints);
    }
}

void MeshInstance::setBlendshapeWeights(const float* v)
{
    if (m_mesh->hasBlendshapes()) {
        if (Globals::getInstance().isStrictUpdateCheckEnabled() && m_blendshape_weights == MakeSpan(v, m_mesh->getBlendshapeCount()))
            return;

        m_blendshape_weights.assign(v, v + m_mesh->getBlendshapeCount());
        markDirty(DirtyFlag::Blendshape);
    }
}

bool MeshInstance::isLightSource() const
{
    if (isEnabled() && getFlag(InstanceFlag::LightSource)) {
        for (auto& pmat : m_materials) {
            if (!pmat)
                continue;
            float3 e = pmat->getEmissive();
            if ((e.x + e.y + e.z) != 0 || pmat->getEmissiveMap())
                return true;
        }
    }
    return false;
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

    auto iroot = m_data.itransform;
    auto bindposes = mesh.getJointBindposes();
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

IMesh*          MeshInstance::getMesh() const  { return m_mesh; }
bool            MeshInstance::isEnabled() const { return m_enabled; }
bool            MeshInstance::getFlag(InstanceFlag f) const { return get_flag(m_data.instance_flags, f); }
IMaterial*      MeshInstance::getMaterial(int slot) const { return m_materials[slot]; }
float4x4        MeshInstance::getTransform() const { return m_data.transform; }
Span<float4x4>  MeshInstance::getJointMatrices() const { return m_joint_matrices; }
Span<float>     MeshInstance::getBlendshapeWeights() const { return m_blendshape_weights; }

const InstanceData& MeshInstance::getData()
{
    m_data.enabled = isEnabled();
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
bool Scene::isEnabled() const { return m_enabled; }
float3 Scene::getBackgroundColor() const { return m_data.bg_color; }


int Scene::getCameraCount() const
{
    return (int)m_cameras.size();
}

ICamera* Scene::getCamera(int i) const
{
    if (i < 0 || i >= m_cameras.size())
        return nullptr;
    return m_cameras[i].get();
}

void Scene::addCamera(ICamera* v)
{
    m_cameras.push_back(base_t(v));
    markDirty(DirtyFlag::Camera);
}

void Scene::removeCamera(ICamera* v)
{
    if (erase(m_cameras, v))
        markDirty(DirtyFlag::Camera);
}

int Scene::getLightCount() const
{
    return (int)m_lights.size();
}

ILight* Scene::getLight(int i) const
{
    if (i < 0 || i >= m_lights.size())
        return nullptr;
    return m_lights[i];
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

int Scene::getInstanceCount() const
{
    return (int)m_instances.size();
}

IMeshInstance* Scene::getInstance(int i) const
{
    if (i < 0 || i >= m_instances.size())
        return nullptr;
    return m_instances[i];
}

void Scene::addInstance(IMeshInstance* v)
{
    m_instances.push_back(base_t(v));
    markDirty(DirtyFlag::Instance);
}

void Scene::removeInstance(IMeshInstance* v)
{
    if (erase(m_instances, v))
        markDirty(DirtyFlag::Instance);
}


const SceneData& Scene::getData()
{
    m_data.samples_per_frame = Globals::getInstance().getSamplesPerFrame();
    m_data.max_trace_depth = Globals::getInstance().getMaxTraceDepth();
    m_data.time = m_timer.elapsed();

    m_data.camera_prev = m_data.camera;
    if (!m_cameras.empty() && m_cameras.front()->isDirty())
        m_data.camera = m_cameras.front()->getData();

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
