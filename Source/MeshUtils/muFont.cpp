#include "pch.h"
#include "muFont.h"

#define muEnableFreetype

#ifdef muEnableFreetype
#include "ft2build.h"
#include FT_FREETYPE_H
#pragma comment(lib, "freetype.lib")
#endif // muEnableFreetype

namespace mu {

struct FontRenderer::Impl
{
    FT_Library  ftlib = nullptr;
    FT_Face     ftface = nullptr;
    Image       image;
};

#ifdef muEnableFreetype

FontRenderer::FontRenderer()
{
    m_impl = std::make_unique<Impl>();
    FT_Init_FreeType(&m_impl->ftlib);
}

FontRenderer::~FontRenderer()
{
    FT_Done_Face(m_impl->ftface);
    FT_Done_FreeType(m_impl->ftlib);
}

bool FontRenderer::loadFontByPath(const char* path)
{
    auto result = FT_New_Face(m_impl->ftlib, path, 0, &m_impl->ftface);
    if (result == FT_Err_Ok) {
        setCharSize(32);
        return true;
    }
    else {
        return false;
    }
}

void FontRenderer::setCharSize(int pt, int dpi)
{
    FT_Set_Char_Size(m_impl->ftface, 0, pt * 64, dpi, dpi);
}

const Image& FontRenderer::render(wchar_t c)
{
    m_impl->image.resize(0, 0, ImageFormat::Ru8);

    auto glyph_index = FT_Get_Char_Index(m_impl->ftface, c);
    if (FT_Load_Glyph(m_impl->ftface, glyph_index, FT_LOAD_DEFAULT) == FT_Err_Ok) {
        if (FT_Render_Glyph(m_impl->ftface->glyph, FT_RENDER_MODE_NORMAL) == FT_Err_Ok) {
            auto& bitmap = m_impl->ftface->glyph->bitmap;
            m_impl->image.resize(bitmap.width, bitmap.rows, ImageFormat::Ru8);
            auto* src = (const unorm8*)bitmap.buffer;
            auto* dst = m_impl->image.data<unorm8>();
            for (uint32_t i = 0; i < bitmap.rows; ++i) {
                memcpy(dst, src, bitmap.width);
                src += bitmap.pitch;
                dst += bitmap.width;
            }
        }
    }
    return m_impl->image;
}

#else

FontRenderer::FontRenderer()
{
    m_impl = std::make_unique<Impl>();
}

FontRenderer::~FontRenderer()
{
}

bool FontRenderer::loadFontByPath(const char* path)
{
    return false;
}

void FontRenderer::setCharSize(int pt, int dpi)
{
}

const Image& FontRenderer::render(wchar_t c)
{
    return m_impl->image;
}

#endif // muEnableFreetype
} // namespace mu
