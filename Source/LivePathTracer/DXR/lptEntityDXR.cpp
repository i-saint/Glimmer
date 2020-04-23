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



} // namespace lpt
#endif // _WIN32
