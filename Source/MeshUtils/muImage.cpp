#include "pch.h"
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

int GetChannelCount(ImageFormat f)
{
    switch (f) {
    case ImageFormat::Ru8: return 1;
    case ImageFormat::RGu8: return 2;
    case ImageFormat::RGBu8: return 3;
    case ImageFormat::RGBAu8: return 4;
    case ImageFormat::Rf16: return 1;
    case ImageFormat::RGf16: return 2;
    case ImageFormat::RGBf16: return 3;
    case ImageFormat::RGBAf16: return 4;
    case ImageFormat::Rf32: return 1;
    case ImageFormat::RGf32: return 2;
    case ImageFormat::RGBf32: return 3;
    case ImageFormat::RGBAf32: return 4;
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
    if (!os)
        return false;

    if (format == ImageFileFormat::Unknown) {
        return false;
    }
    else if (format == ImageFileFormat::EXR) {
        // todo
        return false;
    }
    else {
        int ch = GetChannelCount(m_format);
        int stride = m_width * GetPixelSize(m_format);
        const int jpeg_quality = 90;

        int ret = 0;
        switch (format) {
        case ImageFileFormat::BMP:
            ret = stbi_write_bmp_to_func(swrite, &os, m_width, m_height, ch, m_data.data());
            break;
        case ImageFileFormat::TGA:
            ret = stbi_write_tga_to_func(swrite, &os, m_width, m_height, ch, m_data.data());
            break;
        case ImageFileFormat::JPG:
            ret = stbi_write_jpg_to_func(swrite, &os, m_width, m_height, ch, m_data.data(), jpeg_quality);
            break;
        case ImageFileFormat::PNG:
            ret = stbi_write_png_to_func(swrite, &os, m_width, m_height, ch, m_data.data(), stride);
            break;
        case ImageFileFormat::HDR:
            ret = stbi_write_hdr_to_func(swrite, &os, m_width, m_height, ch, (float*)m_data.data());
            break;
        }
    }

    // todo
    return true;
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
    RGBtoRGBA(ret.getData<D>().data(), src.getData<S>());
    return ret;
}
template<class S, class D>
static inline Image RGBAtoRGB(const Image& src, ImageFormat dst_format)
{
    Image ret(src.getWidth(), src.getHeight(), dst_format);
    RGBAtoRGB(ret.getData<D>().data(), src.getData<S>());
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
    }
    else if (m_format == ImageFormat::RGBf32) {
        if (dst_format == ImageFormat::RGBAf32)
            return RGBtoRGBA<float3, float4>(*this, dst_format);
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
    }
    else if (m_format == ImageFormat::RGBAf32) {
        if (dst_format == ImageFormat::RGBf32)
            return RGBAtoRGB<float4, float3>(*this, dst_format);
        else if (dst_format == ImageFormat::RGBu8)
            return RGBAtoRGB<float4, unorm8x3>(*this, dst_format);
    }
    return Image();
}

} // namespace mu
