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
    int         max_height = 0;
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
    m_impl->char_size = size;
    FT_Set_Pixel_Sizes(m_impl->ftface, 0, size);
    //FT_Set_Char_Size(m_impl->ftface, 0, size << 6, 0, 0);
}

int FontRenderer::getCharSize() const
{
    return m_impl->char_size;
}

bool FontRenderer::render(wchar_t c, Image& dst_image, int2& dst_pos, int2& dst_adance)
{
    dst_image.resize(0, 0, ImageFormat::Ru8);

    auto glyph_index = FT_Get_Char_Index(m_impl->ftface, c);
    if (FT_Load_Glyph(m_impl->ftface, glyph_index, FT_LOAD_DEFAULT) == FT_Err_Ok) {
        auto& glyph = m_impl->ftface->glyph;
        if (FT_Render_Glyph(m_impl->ftface->glyph, FT_RENDER_MODE_NORMAL) == FT_Err_Ok) {
            auto& bitmap = glyph->bitmap;
            m_impl->max_height = std::max<int>(m_impl->max_height, bitmap.rows);

            dst_pos = { glyph->bitmap_left, m_impl->char_size - glyph->bitmap_top };
            dst_adance = { glyph->advance.x >> 6, glyph->advance.y >> 6 };
            dst_image.resize(bitmap.width, bitmap.rows, ImageFormat::Ru8);
            auto* src = (const unorm8*)bitmap.buffer;
            auto* dst = dst_image.data<unorm8>();
            for (uint32_t i = 0; i < bitmap.rows; ++i) {
                memcpy(dst, src, bitmap.width);
                src += bitmap.pitch;
                dst += bitmap.width;
            }
            return true;
        }
    }
    return false;
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

bool FontRenderer::render(wchar_t c, Image& dst_image, int2& dst_pos, int2& dst_adance)
{
    dst_image.resize(0, 0, ImageFormat::Ru8);
    dst_pos = int2{ 0, 0 };
    dst_adance = int2{ 0, 0 };
}

#endif // muEnableFreetype

struct FontAtlas::Impl
{
    Image image;
    Image glyph;
    FontRendererPtr renderer;
    RawVector<GlyphData> glyph_data;
    RawVector<std::pair<int, int>> glyph_table;
    int2 next_pos{};

    Impl()
    {
        glyph_data.reserve(1024);
        glyph_table.reserve(1024);
    }
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
    auto& next_pos = m_impl->next_pos;
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
    int2 glyph_pos, glyph_advance;
    renderer->render(c, m_impl->glyph, glyph_pos, glyph_advance);

    int2 base_pos = next_pos;
    if (base_pos.x + glyph_advance.x > image.getSize().x) {
        // go next line
        if (base_pos.y + renderer->getCharSize() > image.getSize().y)
            return false; // no remaining space
        base_pos.x = 0;
        base_pos.y += renderer->getCharSize();
    }
    next_pos = base_pos + int2{ glyph_advance.x, 0 };

    image.copy(m_impl->glyph, base_pos + glyph_pos);

    // add record
    float2 pixel_size = float2::one() / image.getSize();
    GlyphData gd;
    gd.position = base_pos;
    gd.size = int2{ glyph_advance.x, renderer->getCharSize() };
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
