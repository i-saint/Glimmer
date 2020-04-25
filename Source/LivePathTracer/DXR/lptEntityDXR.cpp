#include "pch.h"
#ifdef _WIN32
#include "Foundation/lptLog.h"
#include "lptEntityDXR.h"

namespace lpt {

RenderTargetDXR::RenderTargetDXR(TextureFormat format, int width, int height)
    : super(format, width, height)
{
}

void* RenderTargetDXR::getDeviceObject()
{
    return m_texture;
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
