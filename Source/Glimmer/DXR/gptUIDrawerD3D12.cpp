#include "pch.h"
#include "gptUIDrawerD3D12.h"

namespace gpt {

template<class Container>
inline typename Container::pointer expand(Container& container, size_t size)
{
    size_t pos = container.size();
    container.resize(pos + size);
    return container.data() + pos;
}

void UIDrawer::setScreen(float2 size)
{
    m_screen_size = size;
}

static void MakeQuadVertices(float2 pos, float2 size, UIVertex (&vdata)[4])
{
    vdata[0].position = pos + float2{      0,      0 };
    vdata[1].position = pos + float2{      0, size.y };
    vdata[2].position = pos + float2{ size.x, size.y };
    vdata[3].position = pos + float2{ size.x,      0 };
    vdata[0].uv = float2{ 0, 0 };
    vdata[1].uv = float2{ 0, 1 };
    vdata[2].uv = float2{ 1, 1 };
    vdata[3].uv = float2{ 1, 0 };
    vdata[0].color = float4::one();
    vdata[1].color = float4::one();
    vdata[2].color = float4::one();
    vdata[3].color = float4::one();
}

void UIDrawer::drawRect(float2 pos, float2 size)
{
    UIDrawCall cmd{};
    cmd.type = UIDrawType::Fill;
    cmd.vertex_count = 4;
    cmd.vertex_offset = (int)m_vertices.size();
    cmd.index_count = 6;
    cmd.index_offset = (int)m_indices.size();
    m_drawcalls.push_back(cmd);

    auto* vertices = expand(m_vertices, cmd.vertex_count);
    auto* indices = expand(m_indices, cmd.index_count);

    UIVertex vdata[4]{};
    MakeQuadVertices(pos, size, vdata);
    std::copy_n(vdata, cmd.vertex_count, vertices);

    const int idata[] = { 0,1,2, 2,3,0 };
    std::copy_n(idata, cmd.index_count, indices);
}

void UIDrawer::drawRectLine(float2 pos, float2 size)
{
    UIDrawCall cmd{};
    cmd.type = UIDrawType::Line;
    cmd.vertex_count = 4;
    cmd.vertex_offset = (int)m_vertices.size();
    cmd.index_count = 8;
    cmd.index_offset = (int)m_indices.size();
    m_drawcalls.push_back(cmd);

    auto* vertices = expand(m_vertices, cmd.vertex_count);
    auto* indices = expand(m_indices, cmd.index_count);

    UIVertex vdata[4]{};
    MakeQuadVertices(pos, size, vdata);
    std::copy_n(vdata, cmd.vertex_count, vertices);

    const int idata[] = { 0,1, 1,2, 2,3, 3,0 };
    std::copy_n(idata, cmd.index_count, indices);
}

void UIDrawer::drawRectTexture(float2 pos, float2 size, ITexture* tex)
{
    UIDrawCall cmd{};
    cmd.type = UIDrawType::Fill;
    cmd.vertex_count = 4;
    cmd.vertex_offset = (int)m_vertices.size();
    cmd.index_count = 6;
    cmd.index_offset = (int)m_indices.size();
    m_drawcalls.push_back(cmd);

    auto* vertices = expand(m_vertices, cmd.vertex_count);
    auto* indices = expand(m_indices, cmd.index_count);

    UIVertex vdata[4]{};
    MakeQuadVertices(pos, size, vdata);
    std::copy_n(vdata, cmd.vertex_count, vertices);

    const int idata[] = { 0,1,2, 2,3,0 };
    std::copy_n(idata, cmd.index_count, indices);
}

void UIDrawer::drawText(float2 pos, char16_t* text, size_t len)
{
    if (!m_font)
        return;

    UIDrawCall cmd{};
    cmd.type = UIDrawType::Text;
    cmd.vertex_count = 4 * (int)len;
    cmd.vertex_offset = (int)m_vertices.size();
    cmd.index_count = 6 * (int)len;
    cmd.index_offset = (int)m_indices.size();
    m_drawcalls.push_back(cmd);

    auto* vertices = expand(m_vertices, cmd.vertex_count);
    auto* indices = expand(m_indices, cmd.index_count);

    float2 unit_size = float2::one() / m_screen_size;
    m_font->makeQuads(text, len, pos, unit_size, &vertices[0].position, &vertices[0].uv, sizeof(UIVertex));

    for (size_t vi = 0; vi < cmd.vertex_count; ++vi)
        vertices[vi].color = float4::one();

    for (size_t qi = 0; qi < len; ++qi) {
        const int idata[] = { 0,1,2, 2,3,0 };
        int offset = 4 * qi;
        for (size_t ii = 0; ii < 6; ++ii)
            indices[ii] = idata[ii] + offset;
        indices += 6;
    }
}



void UIDrawerD3D12::updateResources()
{
    // todo
}

void UIDrawerD3D12::flush()
{
    // todo
}

} // namespace gpt
