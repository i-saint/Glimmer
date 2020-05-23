#include "pch.h"
#include "muAlgorithm.h"
#include "muImage.h"

#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace mu {

int GetPixelSize(ImageFormat f)
{
    int ch = (uint32_t)(f & ImageFormat::ChannelMask);
    switch (f & ImageFormat::TypeMask) {
    case ImageFormat::U8: return ch * 1;
    case ImageFormat::F16: return ch * 2;
    case ImageFormat::F32: return ch * 4;
    default: return 0;
    }
}

int GetChannelCount(ImageFormat f)
{
    return (uint32_t)(f & ImageFormat::ChannelMask);
}

int GetBitsPerChannel(ImageFormat f)
{
    switch (f & ImageFormat::TypeMask) {
    case ImageFormat::U8: return 8;
    case ImageFormat::F16: return 16;
    case ImageFormat::F32: return 32;
    default: return 0;
    }
}

Image::Image()
{
}

Image::Image(int width, int height, ImageFormat format)
{
    resize(width, height, format);
}

void Image::resize(int width, int height, ImageFormat format)
{
    m_width = width;
    m_height = height;
    m_format = format;

    size_t size = width * height * GetPixelSize(format);
    m_data.resize_discard(size);
}

int Image::getWidth() const { return m_width; }
int Image::getHeight() const { return m_height; }
ImageFormat Image::getFormat() const { return m_format; }
size_t Image::getSizeInByte() const { return m_data.size(); }


// IO impl

static int sread(void* user, char* data, int size)
{
    auto* is = (std::istream*)user;
    is->read(data, size);
    return (int)is->gcount();
}
static void sskip(void* user, int n)
{
    auto* is = (std::istream*)user;
    is->ignore(n);
}
static int seof(void* user)
{
    auto* is = (std::istream*)user;
    return is->eof();
}

static void swrite(void* user, void* data, int size)
{
    auto* os = (std::ostream*)user;
    os->write((char*)data, size);
}


static ImageFileFormat PathToImageFileFormat(const char* path)
{
    std::string ext;
    {
        size_t i = std::strlen(path);
        while (i > 0) {
            --i;
            if (path[i] == '.') {
                ext = path + (i + 1);
                for (auto& c : ext)
                    c = (char)std::tolower(c);
                break;
            }
        }
    }

    if (ext == "exr")
        return ImageFileFormat::EXR;
    else if (ext == "bmp")
        return ImageFileFormat::BMP;
    else if (ext == "tga")
        return ImageFileFormat::TGA;
    else if (ext == "jpg" || ext == "jpeg")
        return ImageFileFormat::JPG;
    else if (ext == "png")
        return ImageFileFormat::PNG;
    else if (ext == "gif")
        return ImageFileFormat::GIF;
    else if (ext == "psd")
        return ImageFileFormat::PSD;
    else if (ext == "hdr")
        return ImageFileFormat::HDR;
    else if (ext == "pic")
        return ImageFileFormat::PIC;
    else
        return ImageFileFormat::Unknown;
}


bool Image::read(std::istream& is, ImageFileFormat format)
{
    if (!is)
        return false;

    if (format == ImageFileFormat::Unknown) {
        return false;
    }
    else if (format == ImageFileFormat::EXR) {
        // todo
        return false;
    }
    else {
        stbi__context ctx;
        stbi_io_callbacks cbs{ sread, sskip, seof };
        stbi__start_callbacks(&ctx, &cbs, &is);

        int width = 0;
        int height = 0;
        int num_channels = 0;
        int bits = 0;
        void* data = nullptr;

        switch (format) {
        case ImageFileFormat::HDR:
            data = stbi_loadf_from_callbacks(&cbs, &is, &width, &height, &num_channels, 0);
            bits = 32;
            break;
        default:
            data = stbi_load_from_callbacks(&cbs, &is, &width, &height, &num_channels, 0);
            bits = 8;
            break;
        }

        if (num_channels == 1) {
            if (bits == 8) {
                resize(width, height, ImageFormat::Ru8);
                m_data.assign((char*)data, getSizeInByte());
            }
            else if (bits == 32) {
                resize(width, height, ImageFormat::Rf32);
                m_data.assign((char*)data, getSizeInByte());
            }
        }
        else if (num_channels == 2) {
            if (bits == 8) {
                resize(width, height, ImageFormat::RGu8);
                m_data.assign((char*)data, getSizeInByte());
            }
            else if (bits == 32) {
                resize(width, height, ImageFormat::RGf32);
                m_data.assign((char*)data, getSizeInByte());
            }
        }
        else if (num_channels == 3) {
            if (bits == 8) {
                resize(width, height, ImageFormat::RGBu8);
                m_data.assign((char*)data, getSizeInByte());
            }
            else if (bits == 32) {
                resize(width, height, ImageFormat::RGBf32);
                m_data.assign((char*)data, getSizeInByte());
            }
        }
        else if (num_channels == 4) {
            if (bits == 8) {
                resize(width, height, ImageFormat::RGBAu8);
                m_data.assign((char*)data, getSizeInByte());
            }
            else if (bits == 32) {
                resize(width, height, ImageFormat::RGBAf32);
                m_data.assign((char*)data, getSizeInByte());
            }
        }

        stbi_image_free(data);
        return getSizeInByte() != 0;
    }
}

bool Image::read(const char* path)
{
    auto f = PathToImageFileFormat(path);
    if (f == ImageFileFormat::Unknown)
        return false;

    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    return read(ifs, f);
}


bool Image::write(std::ostream& os, ImageFileFormat format) const
{
    if (!os || format == ImageFileFormat::Unknown)
        return false;

    if (format == ImageFileFormat::EXR) {
        // todo
        return false;
    }
    else {
        int ch = GetChannelCount(m_format);
        //int bits = GetBitsPerChannel(m_format);
        const int jpeg_quality = 90;

        Image tmp;
        int ret = 0;
        switch (format) {
        case ImageFileFormat::BMP:
            tmp = convert(ImageFormat::RGBu8);
            ret = stbi_write_bmp_to_func(swrite, &os, m_width, m_height, 3, tmp.data());
            break;
        case ImageFileFormat::JPG:
            tmp = convert(ImageFormat::RGBu8);
            ret = stbi_write_jpg_to_func(swrite, &os, m_width, m_height, 3, tmp.data(), jpeg_quality);
            break;
        case ImageFileFormat::TGA:
            tmp = convert(ImageFormat::U8 | (m_format & ImageFormat::ChannelMask));
            ret = stbi_write_tga_to_func(swrite, &os, m_width, m_height, ch, tmp.data());
            break;
        case ImageFileFormat::PNG:
            tmp = convert(ImageFormat::U8 | (m_format & ImageFormat::ChannelMask));
            ret = stbi_write_png_to_func(swrite, &os, m_width, m_height, ch, tmp.data(), 0);
            break;
        case ImageFileFormat::HDR:
            tmp = convert(ImageFormat::F32 | (m_format & ImageFormat::ChannelMask));
            ret = stbi_write_hdr_to_func(swrite, &os, m_width, m_height, ch, tmp.data<float>());
            break;
        }
        return ret != 0;
    }
}
bool Image::write(const char* path) const
{
    auto f = PathToImageFileFormat(path);
    if (f == ImageFileFormat::Unknown)
        return false;

    std::ofstream ofs(path, std::ios::out | std::ios::binary);
    return write(ofs, f);
}


// convert impl

template<class S, class D>
static inline Image RGBtoRGBA(const Image& src, ImageFormat dst_format)
{
    Image ret(src.getWidth(), src.getHeight(), dst_format);
    RGBtoRGBA(ret.data<D>(), src.span<S>());
    return ret;
}
template<class S, class D>
static inline Image RGBAtoRGB(const Image& src, ImageFormat dst_format)
{
    Image ret(src.getWidth(), src.getHeight(), dst_format);
    RGBAtoRGB(ret.data<D>(), src.span<S>());
    return ret;
}

template<class D, class S, muEnableIf(S::vector_length == D::vector_length && S::vector_length == 3)>
static inline void Assign(D& d, const S& s)
{
    d = { s[0], s[1], s[2] };
}
template<class D, class S, muEnableIf(S::vector_length == D::vector_length && S::vector_length == 4)>
static inline void Assign(D& d, const S& s)
{
    d = { s[0], s[1], s[2], s[3] };
}

template<class S, class D, muEnableIf(S::vector_length == D::vector_length)>
static inline Image Convert(const Image& src, ImageFormat dst_format)
{
    Image ret(src.getWidth(), src.getHeight(), dst_format);
    auto* s = src.data<S>();
    auto* d = ret.data<D>();
    size_t n = src.getWidth() * src.getHeight();
    for (size_t i = 0; i < n; ++i)
        Assign(d[i], s[i]);
    return ret;
}

Image Image::convert(ImageFormat dst_format) const
{
    if (m_format == dst_format)
        return *this;

    // only RGB <-> RGBA for now
    if (m_format == ImageFormat::RGBu8) {
        if (dst_format == ImageFormat::RGBAu8)
            return RGBtoRGBA<unorm8x3, unorm8x4>(*this, dst_format);
    }
    else if (m_format == ImageFormat::RGBf16) {
        if (dst_format == ImageFormat::RGBAf16)
            return RGBtoRGBA<half3, half4>(*this, dst_format);
        else if (dst_format == ImageFormat::RGBu8)
            return Convert<half3, unorm8x3>(*this, dst_format);
    }
    else if (m_format == ImageFormat::RGBf32) {
        if (dst_format == ImageFormat::RGBAf32)
            return RGBtoRGBA<float3, float4>(*this, dst_format);
        else if (dst_format == ImageFormat::RGBu8)
            return Convert<float3, unorm8x3>(*this, dst_format);
    }
    else if (m_format == ImageFormat::RGBAu8) {
        if (dst_format == ImageFormat::RGBu8)
            return RGBAtoRGB<unorm8x4, unorm8x3>(*this, dst_format);
    }
    else if (m_format == ImageFormat::RGBAf16) {
        if (dst_format == ImageFormat::RGBf16)
            return RGBAtoRGB<half4, half3>(*this, dst_format);
        else if (dst_format == ImageFormat::RGBu8)
            return RGBAtoRGB<half4, unorm8x3>(*this, dst_format);
        else if (dst_format == ImageFormat::RGBAu8)
            return Convert<half4, unorm8x4>(*this, dst_format);
    }
    else if (m_format == ImageFormat::RGBAf32) {
        if (dst_format == ImageFormat::RGBf32)
            return RGBAtoRGB<float4, float3>(*this, dst_format);
        else if (dst_format == ImageFormat::RGBu8)
            return RGBAtoRGB<float4, unorm8x3>(*this, dst_format);
        else if (dst_format == ImageFormat::RGBAu8)
            return Convert<float4, unorm8x4>(*this, dst_format);
    }
    return Image();
}

} // namespace mu
