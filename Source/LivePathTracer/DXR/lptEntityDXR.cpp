#include "pch.h"
#ifdef _WIN32
#include "Foundation/lptLog.h"
#include "Foundation/lptMisc.h"
#include "lptEntityDXR.h"

namespace lpt {


void MeshDXR::clearBLAS()
{
    m_blas = nullptr;
    m_blas_scratch = nullptr;
}


void MeshInstanceDXR::clearBLAS()
{
    // not clear BLAS for deformed vertices because update time is what we want to measure in this case.
    //blas_scratch = nullptr;
    //blas_deformed = nullptr;
    if (m_mesh)
        m_mesh->clearBLAS();
}


CameraDXR* ContextDXR::createCamera()
{
    auto r = new CameraDXR();
    return r;
}

LightDXR* ContextDXR::createLight()
{
    auto r = new LightDXR();
    return r;
}

RenderTargetDXR* ContextDXR::createRenderTarget()
{
    auto r = new RenderTargetDXR();
    return r;
}

TextureDXR* ContextDXR::createTexture()
{
    auto r = new TextureDXR();
    return r;
}

MaterialDXR* ContextDXR::createMaterial()
{
    auto r = new MaterialDXR();
    return r;
}

MeshDXR* ContextDXR::createMesh()
{
    auto r = new MeshDXR();
    return r;
}

MeshInstanceDXR* ContextDXR::createMeshInstance()
{
    auto r = new MeshInstanceDXR();
    return r;
}

SceneDXR* ContextDXR::createScene()
{
    auto r = new SceneDXR();
    return r;
}


} // namespace lpt
#endif // _WIN32
