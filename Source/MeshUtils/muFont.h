#pragma once
#include "muImage.h"

namespace mu {

class FontRenderer
{
public:
    FontRenderer();
    ~FontRenderer();

    bool loadFontByPath(const char* path);
    void setCharSize(int pt, int dpi = 72);
    const Image& render(wchar_t c);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace mu
