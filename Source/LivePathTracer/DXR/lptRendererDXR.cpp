#include "pch.h"
#include "Foundation/lptLog.h"
#include "lptRenderer.h"
#ifdef _WIN32
#include "lptGfxContextDXR.h"

namespace lpt {

class RendererDXR : public RendererBase
{
public:
    RendererDXR();
    ~RendererDXR() override;
    void setName(const std::string& name) override;

    bool initialized() const override;
    bool valid() const override;

    bool isRendering() const override;
    void frameBegin() override; // called from render thread
    void render() override; // called from render thread
    void finish() override; // called from render thread
    void frameEnd() override; // called from render thread

    bool readbackRenderTarget(void *dst) override;
    std::string getTimestampLog() override;
    void* getRenderTexturePtr() override;

private:
    RenderDataDXR m_render_data;
    std::atomic_bool m_is_initialized{ false };
};


RendererDXR::RendererDXR()
{
    GfxContextDXR::initializeInstance();
    m_is_initialized = true;
}

RendererDXR::~RendererDXR()
{
    if (m_is_initialized) {
        GfxContextDXR::finalizeInstance();
    }
}

void RendererDXR::setName(const std::string& name)
{
    m_render_data.name = name;
}

bool RendererDXR::initialized() const
{
    return m_is_initialized;
}

bool RendererDXR::valid() const
{
    if (!this)
        return false;
    auto ctx = GfxContextDXR::getInstance();
    if (!ctx || !ctx->checkError()) {
        m_is_rendering = false;
        return false;
    }
    return true;
}

bool RendererDXR::isRendering() const
{
    return m_is_rendering;
}

void RendererDXR::frameBegin()
{
    if (GetGlobals().hasDebugFlag(DebugFlag::ForceUpdateAS)) {
        // clear static meshes' BLAS
        for (auto& inst : m_render_data.instances_prev)
            inst->clearBLAS();

        // mark updated to update deformable meshes' BLAS
        for (auto& inst : m_render_data.instances_prev)
            inst->base->markUpdated();
    }
}

void RendererDXR::render()
{
    if (!valid() || !m_ready_to_render)
        return;

    if (m_mutex.try_lock()) {
        m_is_rendering = true;
        auto ctx = GfxContextDXR::getInstance();
        ctx->prepare(m_render_data);
        ctx->setSceneData(m_render_data, m_scene_data);
        ctx->setRenderTarget(m_render_data, m_render_target);
        ctx->setMeshes(m_render_data, m_meshes);
        ctx->flush(m_render_data);
        m_mutex.unlock();
    }
}

void RendererDXR::finish()
{
    if (!m_is_rendering)
        return;

    auto ctx = GfxContextDXR::getInstance();
    if (!ctx->finish(m_render_data))
        m_render_data.clear();
    m_is_rendering = false;
}

void RendererDXR::frameEnd()
{
}

bool RendererDXR::readbackRenderTarget(void *dst)
{
    if (!valid())
        return false;

    auto ctx = GfxContextDXR::getInstance();
    return ctx->readbackRenderTarget(m_render_data, dst);
}

std::string RendererDXR::getTimestampLog()
{
    std::string ret;
#ifdef lptEnableTimestamp
    if (m_render_data.timestamp) {
        if (m_mutex.try_lock()) {
            ret = m_render_data.timestamp->getLog();
            m_mutex.unlock();
        }
    }
#endif // lptEnableTimestamp
    return ret;
}

void* RendererDXR::getRenderTexturePtr()
{
    if (m_render_data.render_target)
        return m_render_data.render_target->m_texture->resource;
    return nullptr;
}

IRenderer* CreateRendererDXR()
{
    auto ret = new RendererDXR();
    if (ret->initialized() && !ret->valid()) {
        ret->release();
        ret = nullptr;
    }
    return ret;
}

} // namespace lpt

#else // _WIN32

namespace lpt {

IRenderer* CreateRendererDXR()
{
    return nullptr;
}

} // namespace lpt

#endif // _WIN32
