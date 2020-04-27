#pragma once
#include "lptInterface.h"

namespace lpt {

class IWindow : public IEntity
{
public:
    virtual bool open(int width, int height) = 0;
    virtual void close() = 0;
    virtual void processMessages() = 0;

    virtual bool isClosed() = 0;
    virtual void* getHandle() = 0;
};
using IWindowPtr = ref_ptr<IWindow>;

} // namespace lpt

lptAPI lpt::IWindow* lptCreateWindow_(int width, int height);

inline lpt::IWindowPtr lptCreateWindow(int width, int height)
{
    return lpt::IWindowPtr(lptCreateWindow_(width, height));
}
