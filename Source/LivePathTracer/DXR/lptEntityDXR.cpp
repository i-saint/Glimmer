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
    m_blas_scratch = nullptr;
    m_blas_deformed = nullptr;
}



} // namespace lpt
#endif // _WIN32
