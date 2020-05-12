#pragma once
#include "muHalf.h"
#include "muMath.h"
#include "muRawVector.h"

namespace mu {

enum class ImageFormat : uint32_t
{
    Unknown,
    Ru8,
    RGu8,
    RGBu8,
    RGBAu8,
    Rf16,
    RGf16,
    RGBf16,
    RGBAf16,
    Rf32,
    RGf32,
    RGBf32,
    RGBAf32,
};

enum class ImageFileFormat : uint32_t
{
    Unknown,
    BMP,
    TGA,
    JPG,
    PNG,
    PSD,
    GIF,
    HDR,
    PIC,
    EXR,
};

inline int GetPixelSize(ImageFormat f)
{
    switch (f) {
    case ImageFormat::Ru8: return 1;
    case ImageFormat::RGu8: return 2;
    case ImageFormat::RGBu8: return 3;
    case ImageFormat::RGBAu8: return 4;
    case ImageFormat::Rf16: return 2;
    case ImageFormat::RGf16: return 4;
    case ImageFormat::RGBf16: return 6;
    case ImageFormat::RGBAf16: return 8;
    case ImageFormat::Rf32: return 4;
    case ImageFormat::RGf32: return 8;
    case ImageFormat::RGBf32: return 12;
    case ImageFormat::RGBAf32: return 16;
    default: return 0;
    }
}

inline float4 Color32toFloat4(uint32_t c)
{
    return{
        (float)(c & 0xff) / 255.0f,
        (float)((c & 0xff00) >> 8) / 255.0f,
        (float)((c & 0xff0000) >> 16) / 255.0f,
        (float)(c >> 24) / 255.0f,
    };
}
inline uint32_t Float4toColor32(const float4& c)
{
    return
        (int)(c.x * 255.0f) |
        ((int)(c.y * 255.0f) << 8) |
        ((int)(c.z * 255.0f) << 16) |
        ((int)(c.w * 255.0f) << 24);
}

template<class RGBA, class ABGR>
inline void ABGRtoRGBA(RGBA* dst, const Span<ABGR>& src)
{
    size_t n = src.size();
    for (size_t pi = 0; pi < n; ++pi) {
        auto t = src[pi];
        dst[pi] = { t[3], t[2], t[1], t[0] };
    }
}

template<class RGBA, class ARGB>
inline void ARGBtoRGBA(RGBA* dst, const Span<ARGB>& src)
{
    size_t n = src.size();
    for (size_t pi = 0; pi < n; ++pi) {
        auto t = src[pi];
        dst[pi] = { t[1], t[2], t[3], t[0] };
    }
}

template<class RGBA, class BGRA>
inline void BGRAtoRGBA(RGBA* dst, const Span<BGRA>& src)
{
    size_t n = src.size();
    for (size_t pi = 0; pi < n; ++pi) {
        auto t = src[pi];
        dst[pi] = { t[2], t[1], t[0], t[3] };
    }
}

template<class RGB, class BGR>
inline void BGRtoRGB(RGB* dst, const Span<BGR>& src)
{
    size_t n = src.size();
    for (size_t pi = 0; pi < n; ++pi) {
        auto t = src[pi];
        dst[pi] = { t[2], t[1], t[0] };
    }
}

template<class RGBA, class BGR>
inline void BGRtoRGBA(RGBA* dst, const Span<BGR>& src, typename RGBA::scalar_t a = 0)
{
    size_t n = src.size();
    for (size_t pi = 0; pi < n; ++pi) {
        auto t = src[pi];
        dst[pi] = { t[2], t[1], t[0], a };
    }
}

template<class RGB, class RG>
inline void RGtoRGB(RGB* dst, const Span<RG>& src, typename RGB::scalar_t b = 0)
{
    size_t n = src.size();
    for (size_t pi = 0; pi < n; ++pi) {
        auto t = src[pi];
        dst[pi] = { t[0], t[1], b };
    }
}

template<class RGBA, class RGB>
inline void RGBtoRGBA(RGBA* dst, const Span<RGB>& src, typename RGBA::scalar_t a = 0)
{
    size_t n = src.size();
    for (size_t pi = 0; pi < n; ++pi) {
        auto t = src[pi];
        dst[pi] = { t[0], t[1], t[2], a };
    }
}

template<class RGB, class RGBA>
inline void RGBAtoRGB(RGB* dst, const Span<RGBA>& src)
{
    size_t n = src.size();
    for (size_t pi = 0; pi < n; ++pi) {
        auto t = src[pi];
        dst[pi] = { t[0], t[1], t[2] };
    }
}


class Image
{
public:
    Image();
    Image(int width, int height, ImageFormat format);
    void resize(int width, int height, ImageFormat format);

    int getWidth() const;
    int getHeight() const;
    ImageFormat getFormat() const;
    size_t getSizeInByte() const;

    template<class T = char>
    Span<T> getData() const { return MakeSpan((const T*)m_data.data(), m_data.size() / sizeof(T)); }

    bool read(std::istream& is, ImageFileFormat format);
    bool read(const char* path);

    bool write(std::ostream& is, ImageFileFormat format) const;
    bool write(const char* path) const;

    Image convert(ImageFormat dst_format) const;

private:
    int m_width = 0;
    int m_height = 0;
    ImageFormat m_format = ImageFormat::Unknown;
    RawVector<char> m_data;
};

} // namespace mu
