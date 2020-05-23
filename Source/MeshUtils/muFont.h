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
    const Image& render(wchar_t c);

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
    int addString(const wchar_t* str, size_t len);
    bool addCharacter(wchar_t c);

    const Image& getImage() const;
    const GlyphData& getGplyph(wchar_t c) const;


private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
using GlyphManagerPtr = std::shared_ptr<FontAtlas>;

} // namespace mu
