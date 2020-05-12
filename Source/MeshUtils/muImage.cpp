#include "pch.h"
#include "muImage.h"

#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

namespace mu {

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
        int comp = 0;
        void* data = nullptr;
        stbi__result_info result;

        switch (format) {
        case ImageFileFormat::BMP:
            data = stbi__bmp_load(&ctx, &width, &height, &comp, 0, &result);
            break;
        case ImageFileFormat::TGA:
            data = stbi__tga_load(&ctx, &width, &height, &comp, 0, &result);
            break;
        case ImageFileFormat::PNG:
            data = stbi__png_load(&ctx, &width, &height, &comp, 0, &result);
            break;
        case ImageFileFormat::JPG:
            data = stbi__jpeg_load(&ctx, &width, &height, &comp, 0, &result);
            break;
        case ImageFileFormat::PIC:
            data = stbi__pic_load(&ctx, &width, &height, &comp, 0, &result);
            break;
        case ImageFileFormat::PSD:
            data = stbi__psd_load(&ctx, &width, &height, &comp, 0, &result, 8);
            break;
        case ImageFileFormat::HDR:
            data = stbi__hdr_load(&ctx, &width, &height, &comp, 0, &result);
            break;
        default:
            break;
        }

        if (result.num_channels == 1) {
            if (result.bits_per_channel == 8) {
                resize(width, height, ImageFormat::Ru8);
                m_data.assign((char*)data, getSizeInByte());
            }
            else if (result.bits_per_channel == 32) {
                resize(width, height, ImageFormat::Rf32);
                m_data.assign((char*)data, getSizeInByte());
            }
        }
        else if (result.num_channels == 3) {
            if (result.bits_per_channel == 8) {
                resize(width, height, ImageFormat::RGBu8);
                m_data.assign((char*)data, getSizeInByte());
            }
            else if (result.bits_per_channel == 32) {
                resize(width, height, ImageFormat::RGBf32);
                m_data.assign((char*)data, getSizeInByte());
            }
        }
        else if (result.num_channels == 4) {
            if (result.bits_per_channel == 8) {
                resize(width, height, ImageFormat::RGBAu8);
                m_data.assign((char*)data, getSizeInByte());
            }
            else if (result.bits_per_channel == 32) {
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

template<class D, class S>
static inline Image RGBtoRGBA(const Image& src, ImageFormat dst_format)
{
    Image ret(src.getWidth(), src.getHeight(), dst_format);
    RGBtoRGBA(ret.getData<D>().data(), src.getData<S>());
    return ret;
}
template<class D, class S>
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
            return RGBtoRGBA<unorm8x4, unorm8x3>(*this, dst_format);
    }
    else if (m_format == ImageFormat::RGBf16) {
        if (dst_format == ImageFormat::RGBAf16)
            return RGBtoRGBA<half4, half3>(*this, dst_format);
    }
    else if (m_format == ImageFormat::RGBf32) {
        if (dst_format == ImageFormat::RGBAf32)
            return RGBtoRGBA<float4, float3>(*this, dst_format);
    }
    else if (m_format == ImageFormat::RGBAu8) {
        if (dst_format == ImageFormat::RGBu8)
            return RGBAtoRGB<unorm8x3, unorm8x4>(*this, dst_format);
    }
    else if (m_format == ImageFormat::RGBAf16) {
        if (dst_format == ImageFormat::RGBf16)
            return RGBAtoRGB<half3, half4>(*this, dst_format);
    }
    else if (m_format == ImageFormat::RGBAf32) {
        if (dst_format == ImageFormat::RGBf32)
            return RGBAtoRGB<float3, float4>(*this, dst_format);
    }
    return Image();
}

} // namespace mu
