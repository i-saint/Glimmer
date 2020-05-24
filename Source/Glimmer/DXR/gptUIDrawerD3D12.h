#pragma once
#include "gptInterface.h"
#include "gptEntity.h"
#include "gptInternalDXR.h"

namespace gpt {

class IUIDrawer : public IObject
{
public:
    virtual void setScreen(float2 size) = 0;
    virtual void drawRect(float2 pos, float2 size) = 0;
    virtual void drawRectLine(float2 pos, float2 size) = 0;
    virtual void drawRectTexture(float2 pos, float2 size, ITexture* tex) = 0;
    virtual void drawText(float2 pos, char16_t* text, size_t len) = 0;
};


enum class UIDrawType : uint32_t
{
    Fill,
    Line,
    Text,
};

struct UIDrawCall
{
    UIDrawType type;
    int vertex_count;
    int vertex_offset;
    int index_count;
    int index_offset;
    ITexture* texture;
};

struct UIVertex
{
    float2 position;
    float2 uv;
    float4 color;
};

class UIDrawer : public IUIDrawer
{
public:
    void setScreen(float2 size) override;
    void drawRect(float2 pos, float2 size) override;
    void drawRectLine(float2 pos, float2 size) override;
    void drawRectTexture(float2 pos, float2 size, ITexture* tex) override;
    void drawText(float2 pos, char16_t* text, size_t len) override;

protected:
    float2 m_screen_size{};
    mu::FontAtlasPtr m_font;

    RawVector<UIVertex>     m_vertices;
    RawVector<int>          m_indices;
    RawVector<UIDrawCall>   m_drawcalls;
};


class UIDrawerD3D12 : public UIDrawer
{
public:
    void updateResources();
    void flush();

protected:
    ID3D12ResourcePtr m_vertex_buffer;
    ID3D12ResourcePtr m_index_buffer;
    DescriptorHandleDXR m_srv_vertex_buffer;
    DescriptorHandleDXR m_srv_index_buffer;
};

} // namespace gpt
