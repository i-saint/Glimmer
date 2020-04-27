#include "pch.h"
#ifdef _WIN32
#include "Foundation/lptLog.h"
#include "lptEntityDXR.h"
#include "lptContextDXR.h"

namespace lpt {

RenderTargetDXR::RenderTargetDXR(TextureFormat format, int width, int height)
    : super(format, width, height)
{
}

bool RenderTargetDXR::readback(void* dst)
{
    if (m_readback_enabled && m_buf_readback && dst) {
        m_context->readbackTexture(dst, m_buf_readback, m_width, m_height, GetDXGIFormat(m_format));
        return true;
    }
    else {
        return false;
    }
}

void* RenderTargetDXR::getDeviceObject()
{
    return m_frame_buffer;
}

TextureDXR::TextureDXR(TextureFormat format, int width, int height)
    : super(format, width, height)
{
}

void* TextureDXR::getDeviceObject()
{
    return m_texture;
}

void MeshDXR::clearBLAS()
{
    m_blas = nullptr;
    m_blas_scratch = nullptr;
}


MeshInstanceDXR::MeshInstanceDXR(IMesh* v)
    : super(v)
{
}

void MeshInstanceDXR::clearBLAS()
{
    m_blas_scratch = nullptr;
    m_blas_deformed = nullptr;
}



} // namespace lpt
#endif // _WIN32
