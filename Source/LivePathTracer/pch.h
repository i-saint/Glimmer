#pragma once

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <dxgiformat.h>
#include <dxcapi.h>
#include <comdef.h>
#endif // _WIN32

#include <array>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <algorithm>
#include <functional>
#include <memory>
#include <iostream>
#include <sstream>
#include <fstream>
#include <thread>
#include <future>
#include <random>
#include <regex>
#include <iterator>
#include <locale>
#include <codecvt>
#include <chrono>
#include <iomanip>
#include <cassert>

#include "MeshUtils/MeshUtils.h"
#define lptImpl


namespace lpt {

using mu::float2;
using mu::float3;
using mu::float4;
using mu::quatf;
using mu::float2x2;
using mu::float3x3;
using mu::float3x4;
using mu::float4x4;

using mu::int2;
using mu::int3;
using mu::int4;

} // namespace lpt
