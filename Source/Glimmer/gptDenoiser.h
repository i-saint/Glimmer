#pragma once
#include "gptEntity.h"

namespace gpt {

class IDenoiser : public IObject
{
public:
    virtual void resize(int width, int height) = 0;
    virtual void denoise(ITexture *dst, ITexture* src) = 0;
    virtual void finish() = 0;
};
using IDenoiserPtr = ref_ptr<IDenoiser>;


IDenoiserPtr gptCreateDenoiserOptiX(IContext* ctx);

} // namespace gpt
