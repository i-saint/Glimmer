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
    int         char_size = 0;
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

bool FontRenderer::loadFontFile(const char* path)
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

bool FontRenderer::loadFontMemory(const void* data, size_t size)
{
    auto result = FT_New_Memory_Face(m_impl->ftlib, (FT_Byte*)data, (FT_Long)size, 0, &m_impl->ftface);
    if (result == FT_Err_Ok) {
        setCharSize(32);
        return true;
    }
    else {
        return false;
    }
}

void FontRenderer::setCharSize(int size)
{
    const int dpi = 72;
    m_impl->char_size = size;
    FT_Set_Char_Size(m_impl->ftface, 0, size * dpi, dpi, dpi);
}

int FontRenderer::getCharSize() const
{
    return m_impl->char_size;
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

#else // muEnableFreetype

FontRenderer::FontRenderer()
{
    m_impl = std::make_unique<Impl>();
}

FontRenderer::~FontRenderer()
{
}

bool FontRenderer::loadFontFile(const char* path)
{
    return false;
}

bool FontRenderer::loadFontMemory(const void* data, size_t size)
{
    return false;
}

void FontRenderer::setCharSize(int pt, int dpi)
{
}

int FontRenderer::getCharSize() const
{
    return 0;
}

const Image& FontRenderer::render(wchar_t c)
{
    return m_impl->image;
}

#endif // muEnableFreetype

struct FontAtlas::Impl
{
    Image image;
    FontRendererPtr renderer;
    RawVector<GlyphData> glyph_data;
    RawVector<std::pair<int, int>> glyph_table;
    int2 last_pos{};
};

FontAtlas::FontAtlas()
{
    m_impl = std::make_unique<Impl>();
}

FontAtlas::~FontAtlas()
{
}

void FontAtlas::setImageSize(int width, int height)
{
    m_impl->image.resize(width, height, ImageFormat::Ru8);
}

void FontAtlas::setFontRenderer(FontRendererPtr renderer)
{
    m_impl->renderer = renderer;
}

int FontAtlas::addString(const wchar_t* str, size_t len)
{
    int ret = 0;
    for (int i = 0; i < len; ++i) {
        if (addCharacter(str[i]))
            ++ret;
    }
    return ret;
}

bool FontAtlas::addCharacter(wchar_t c)
{
    auto& renderer = m_impl->renderer;
    auto& image = m_impl->image;
    auto& last_pos = m_impl->last_pos;
    if (!renderer)
        return false;
    if (image.empty())
        image.resize(4096, 4096, ImageFormat::Ru8);

    // skip if the character already recorded
    int code = (int)c;
    auto it = std::lower_bound(m_impl->glyph_table.begin(), m_impl->glyph_table.end(), code, [](auto& a, int b) {
        return a.first < b;
    });
    if (it != m_impl->glyph_table.end() && it->first == code)
        return false;

    // render glyph
    auto& font = renderer->render(c);
    auto pos = last_pos;
    last_pos.x += font.getSize().x;
    if (last_pos.x > image.getSize().x) {
        if (last_pos.y + renderer->getCharSize() > image.getSize().y)
            return false; // no remaining space
        last_pos.x = 0;
        last_pos.y += renderer->getCharSize();
    }
    image.copy(font, pos);

    // add record
    float2 pixel_size = float2::one() / image.getSize();
    GlyphData gd;
    gd.position = pos;
    gd.size = image.getSize();
    gd.uv_position = pixel_size * gd.position;
    gd.uv_size = pixel_size * gd.size;

    m_impl->glyph_table.push_back({ code, (int)m_impl->glyph_data.size() });
    m_impl->glyph_data.push_back(gd);
    std::sort(m_impl->glyph_table.begin(), m_impl->glyph_table.end(), [](auto& a, auto& b) {
        return a.first < b.first;
    });
    return true;
}

const Image& FontAtlas::getImage() const
{
    return m_impl->image;
}

const FontAtlas::GlyphData& FontAtlas::getGplyph(wchar_t c) const
{
    int code = (int)c;
    auto it = std::lower_bound(m_impl->glyph_table.begin(), m_impl->glyph_table.end(), code, [](auto& a, int b) {
        return a.first < b;
    });
    if (it != m_impl->glyph_table.end() && it->first == code)
        return m_impl->glyph_data[it->second];

    static GlyphData s_dummy{};
    return s_dummy;
}

} // namespace mu
