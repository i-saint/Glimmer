#pragma once
#include "muHalf.h"
#include "muMath.h"
#include "muRawVector.h"

namespace mu {

enum class ImageFormat : uint32_t
{
    Unknown,
    Ru8     = 0x10 | 0x01,
    RGu8    = 0x10 | 0x02,
    RGBu8   = 0x10 | 0x03,
    RGBAu8  = 0x10 | 0x04,
    Rf16    = 0x20 | 0x01,
    RGf16   = 0x20 | 0x02,
    RGBf16  = 0x20 | 0x03,
    RGBAf16 = 0x20 | 0x04,
    Rf32    = 0x30 | 0x01,
    RGf32   = 0x30 | 0x02,
    RGBf32  = 0x30 | 0x03,
    RGBAf32 = 0x30 | 0x04,

    U8      = 0x10,
    F16     = 0x20,
    F32     = 0x30,
    ChannelMask = 0x0f,
    TypeMask = 0xf0,
};
inline ImageFormat operator|(ImageFormat a, ImageFormat b) { return (ImageFormat)((uint32_t)a | (uint32_t)b); }
inline ImageFormat operator&(ImageFormat a, ImageFormat b) { return (ImageFormat)((uint32_t)a & (uint32_t)b); }

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

int GetPixelSize(ImageFormat f);
int GetChannelCount(ImageFormat f);

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

    bool empty() const;
    void resize(int width, int height, ImageFormat format);

    int2 getSize() const;
    ImageFormat getFormat() const;
    size_t getSizeInByte() const;

    template<class T = char> T* data() { return (T*)m_data.data(); }
    template<class T = char> const T* data() const { return (const T*)m_data.data(); }
    template<class T = char> Span<T> span() const { return MakeSpan((T*)m_data.data(), m_data.size() / sizeof(T)); }

    bool copy(const Image& img, int2 position);

    bool read(std::istream& is, ImageFileFormat format);
    bool read(const char* path);

    bool write(std::ostream& is, ImageFileFormat format) const;
    bool write(const char* path) const;

    Image convert(ImageFormat dst_format) const;

private:
    int2 m_size{ 0, 0 };
    ImageFormat m_format = ImageFormat::Unknown;
    RawVector<char> m_data;
};

} // namespace mu
