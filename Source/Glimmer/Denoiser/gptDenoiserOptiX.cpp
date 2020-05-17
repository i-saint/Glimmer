#include "pch.h"
#include "gptDenoiser.h"

#define gptWithOptiX

#ifdef gptWithOptiX
#include <optix.h>

namespace gpt {

class DenoiserOptiX : public RefCount<IDenoiser>
{
public:
    DenoiserOptiX(IContext* ctx);
    ~DenoiserOptiX();
    void resize(int width, int height) override;
    void denoise(ITexture* dst, ITexture* src) override;
    void finish() override;

    void onRefCountZero() override;

    bool valid() const;

private:
    IContext* m_host = nullptr;
    OptixDeviceContext m_context = nullptr;
    OptixDenoiserParams m_params{};
    OptixDenoiser m_denoiser = nullptr;
    OptixImage2D m_input;
    OptixImage2D m_output;
};


DenoiserOptiX::DenoiserOptiX(IContext* ctx)
    : m_host(ctx)
{
    // todo
    // optixDeviceContextCreate(CUcontext fromContext, const OptixDeviceContextOptions ∗options, OptixDeviceContext ∗context);
    // optixDenoiserCreate(OptixDeviceContext context, const OptixDenoiserOptions* options, OptixDenoiser* denoiser);
}

DenoiserOptiX::~DenoiserOptiX()
{
    // todo
    //optixDenoiserDestroy(m_denoiser);
    //optixDeviceContextDestroy(m_context);
}

void DenoiserOptiX::resize(int width, int height)
{
    // todo
}

void DenoiserOptiX::denoise(ITexture* dst, ITexture* src)
{
    // todo
    //optixDenoiserInvoke(m_denoiser, 0, &m_params, );
}

void DenoiserOptiX::finish()
{
    // todo
}

void DenoiserOptiX::onRefCountZero()
{
    delete this;
}

bool DenoiserOptiX::valid() const
{
    return m_context != nullptr;
}


IDenoiserPtr gptCreateDenoiserOptiX(IContext* ctx)
{
    auto ret = new DenoiserOptiX(ctx);
    if (!ret->valid()) {
        delete ret;
        ret = nullptr;
    }
    return ret;
}

} // namespace gpt

#else

namespace gpt {

IDenoiserPtr gptCreateDenoiserOptiX(IContext* ctx)
{
    return nullptr;
}

} // namespace gpt

#endif
