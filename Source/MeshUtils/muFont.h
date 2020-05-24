#pragma once
#include "muImage.h"

namespace mu {

class FontRenderer
{
public:
    FontRenderer();
    ~FontRenderer();

    bool loadFontFile(const char* path);
    bool loadFontMemory(const void* data, size_t size);
    void setCharSize(int size);
    int getCharSize() const;
    bool render(char32_t c, Image& dst_image, int2& dst_pos, int2& dst_adance);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
using FontRendererPtr = std::shared_ptr<FontRenderer>;


class FontAtlas
{
public:
    struct GlyphData
    {
        int2 position;
        int2 size;
        float2 uv_position;
        float2 uv_size;
    };

    FontAtlas();
    ~FontAtlas();

    // default: 4096, 4096
    void setImageSize(int width, int height);
    void setFontRenderer(FontRendererPtr renderer);
    bool addCharacter(char32_t c);
    template<class CharT> int addString(const CharT* str, size_t len);

    const Image& getImage() const;
    const GlyphData& getGlyph(char32_t c) const;

    float makeQuad(char32_t c, float2 base_pos, float2 unit_size, float2* dst_points, float2* dst_uv);
    template<class CharT> float makeQuads(const CharT* str, size_t len, float2 base_pos, float2 unit_size, float2* dst_points, float2* dst_uv);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
using GlyphManagerPtr = std::shared_ptr<FontAtlas>;

} // namespace mu
