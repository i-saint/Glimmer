#pragma once

namespace gpt {

enum class DeviceType : uint32_t
{
    CPU,
    DXR,
};

enum class Format : uint32_t
{
    Unknown,
    Ru8,
    RGu8,
    RGBAu8,
    Rf16,
    RGf16,
    RGBAf16,
    Rf32,
    RGf32,
    RGBAf32,
};

enum class LightType : uint32_t
{
    Directional,
    Point,
    Spot,
    Mesh,
};

enum class MaterialType : uint32_t
{
    Opaque,
    AlphaToCoverage,
    Transparent,
    Translucent,
};

enum class InstanceFlag : uint32_t
{
    Visible     = 0x00000001,
    CastShadows = 0x00000002,
    LightSource = 0x00000004, // internal
    CullFront   = 0x00000008,
    CullBack    = 0x00000010,

    Default = Visible | CastShadows,
};


enum class WindowFlag : uint32_t
{
    None            = 0x00000000,
    Resizable       = 0x00000001,
    MinimizeButton  = 0x00000002,
    MaximizeButton  = 0x00000004,
    Default = Resizable,
};
inline WindowFlag operator|(WindowFlag a, WindowFlag b) { return (WindowFlag)((uint32_t)a | (uint32_t)b); }
inline WindowFlag operator&(WindowFlag a, WindowFlag b) { return (WindowFlag)((uint32_t)a & (uint32_t)b); }

#ifndef gptImpl
struct float2
{
    float x, y;

    float& operator[](int i) { return ((float*)this)[i]; }
    const float& operator[](int i) const { return ((float*)this)[i]; }

    static constexpr float2 zero() { return{ 0, 0 }; }
};

struct float3
{
    float x, y, z;

    float& operator[](int i) { return ((float*)this)[i]; }
    const float& operator[](int i) const { return ((float*)this)[i]; }

    static constexpr float3 zero() { return{ 0, 0, 0 }; }
    static constexpr float3 up() { return{ 0, 1, 0 }; }
};

struct float4
{
    float x, y, z, w;

    float& operator[](int i) { return ((float*)this)[i]; }
    const float& operator[](int i) const { return ((float*)this)[i]; }

    static constexpr float4 zero() { return{ 0, 0, 0, 0 }; }
};
using quatf = float4;

struct float4x4
{
    float4 v[4];

    float4& operator[](int i) { return v[i]; }
    const float4& operator[](int i) const { return v[i]; }

    static constexpr float4x4 identity()
    {
        return{ {
            { 1, 0, 0, 0 },
            { 0, 1, 0, 0 },
            { 0, 0, 1, 0 },
            { 0, 0, 0, 1 },
        } };
    }
};

// equivalent to std::span in C++20
template<class T>
class Span
{
public:
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = pointer;
    using const_iterator = const_pointer;

    Span() {}
    template<size_t N> Span(const T(&d)[N]) : m_data(const_cast<T*>(d)), m_size(N) {}
    Span(const T* d, size_t s) : m_data(const_cast<T*>(d)), m_size(s) {}
    Span(const Span& v) : m_data(const_cast<T*>(v.m_data)), m_size(v.m_size) {}
    template<class Container> Span(const Container& v) : m_data(const_cast<T*>(v.data())), m_size(v.size()) {}
    Span& operator=(const Span& v) { m_data = const_cast<T*>(v.m_data); m_size = v.m_size; return *this; }

    bool empty() const { return m_size == 0; }
    size_t size() const { return m_size; }
    size_t size_bytes() const { return sizeof(T) * m_size; }

    T* data() { return m_data; }
    const T* data() const { return m_data; }

    T& operator[](size_t i) { return m_data[i]; }
    const T& operator[](size_t i) const { return m_data[i]; }

    T& front() { return m_data[0]; }
    const T& front() const { return m_data[0]; }
    T& back() { return m_data[m_size - 1]; }
    const T& back() const { return m_data[m_size - 1]; }

    iterator begin() { return m_data; }
    const_iterator begin() const { return m_data; }
    iterator end() { return m_data + m_size; }
    const_iterator end() const { return m_data + m_size; }

private:
    T* m_data = nullptr;
    size_t m_size = 0;
};
#endif

struct JointWeight
{
    float weight;
    int index;
};


template<class T>
class releaser
{
public:
    static void addRef(T* v) { v->addRef(); }
    static void release(T* v) { v->release(); }
};

template<class T, class Releaser = releaser<T>>
class ref_ptr
{
public:
    ref_ptr() {}
    ref_ptr(T* p) { reset(p); }
    ref_ptr(T&& p) { swap(p); }
    ref_ptr(const ref_ptr& v) { reset(v.m_ptr); }
    ref_ptr& operator=(const ref_ptr& v) { reset(v.m_ptr); return *this; }
    ~ref_ptr() { reset(); }

    void reset(T* p = nullptr)
    {
        if (m_ptr)
            Releaser::release(m_ptr);
        m_ptr = p;
        if (m_ptr)
            Releaser::addRef(m_ptr);
    }

    void swap(ref_ptr& v)
    {
        std::swap(m_ptr, v->m_data);
    }

    T* get() const { return m_ptr; }

    T& operator*() { return *m_ptr; }
    const T& operator*() const { return *m_ptr; }
    T* operator->() { return m_ptr; }
    const T* operator->() const { return m_ptr; }
    operator T* () const { return m_ptr; }
    operator bool() const { return m_ptr; }
    bool operator==(const ref_ptr<T>& v) const { return m_ptr == v.m_ptr; }
    bool operator!=(const ref_ptr<T>& v) const { return m_ptr != v.m_ptr; }

private:
    T* m_ptr = nullptr;
};

class IObject;
class ITexture;
class IRenderTarget;
class IMaterial;
class ILight;
class ICamera;
class IBlendshapeFrame;
class IBlendshape;
class IMesh;
class IMeshInstance;
class IScene;
class IContext;
class IWindowCallback;
class IWindow;


class IGlobals
{
public:
    virtual void enableGenerateTangents(bool v) = 0;
    virtual void enableStrictUpdateCheck(bool v) = 0;
    virtual void enableTimestamp(bool v) = 0;
    virtual void enablePowerStableState(bool v) = 0;
    virtual void enableForceUpdateAS(bool v) = 0;
    virtual void setSamplesPerFrame(int v) = 0;
    virtual void setMaxTraceDepth(int v) = 0;
protected:
    virtual ~IGlobals() {}
};


class IObject
{
public:
    virtual int getID()const = 0;
    virtual int addRef() = 0;
    virtual int release() = 0;
    virtual int getRef() const = 0;
    virtual void setName(const char* name) = 0;
    virtual const char* getName() const = 0;
protected:
    virtual ~IObject() {}
};


class ITexture : public IObject
{
public:
    // actual upload will be done in IContext::render()
    // but the data is copied in upload() and so src can be discarded after calling this.
    virtual void upload(const void* src) = 0;

    virtual int         getWidth() const = 0;
    virtual int         getHeight() const = 0;
    virtual Format      getFormat() const = 0;
    virtual Span<char>  getData() const = 0;
    virtual void*       getDeviceObject() const = 0;
};
using ITexturePtr = ref_ptr<ITexture>;


class IRenderTarget : public IObject
{
public:
    virtual void enableReadback(bool v) = 0;

    // need to be enableReadback(true) before IContext::render().
    // should be called after IContext::finish()
    virtual bool readback(void* dst) = 0;

    virtual int         getWidth() const = 0;
    virtual int         getHeight() const = 0;
    virtual Format      getFormat() const = 0;
    virtual IWindow*    getWindow() const = 0;
    virtual void*       getDeviceObject() const = 0;
};
using IRenderTargetPtr = ref_ptr<IRenderTarget>;


class IMaterial : public IObject
{
public:
    virtual void setType(MaterialType v) = 0;
    virtual void setDiffuse(float3 v) = 0;
    virtual void setRoughness(float v) = 0;
    virtual void setEmissive(float3 v) = 0;
    virtual void setOpacity(float v) = 0;
    virtual void setRefractionIndex(float v) = 0;
    virtual void setDiffuseMap(ITexture* v) = 0;
    virtual void setOpacityMap(ITexture* v) = 0;
    virtual void setRoughnessMap(ITexture* v) = 0;
    virtual void setEmissiveMap(ITexture* v) = 0;
    virtual void setNormalMap(ITexture* v) = 0;

    virtual MaterialType getType() const = 0;
    virtual float3       getDiffuse() const = 0;
    virtual float        getRoughness() const = 0;
    virtual float3       getEmissive() const = 0;
    virtual float        getOpacity() const = 0;
    virtual float        getRefractionIndex() const = 0;
    virtual ITexture*    getDiffuseMap() const = 0;
    virtual ITexture*    getOpacityMap() const = 0;
    virtual ITexture*    getRoughnessMap() const = 0;
    virtual ITexture*    getEmissiveMap() const = 0;
    virtual ITexture*    getNormalMap() const = 0;
};
using IMaterialPtr = ref_ptr<IMaterial>;


class ILight : public IObject
{
public:
    virtual void setEnabled(bool v) = 0;
    virtual void setType(LightType v) = 0;
    virtual void setPosition(float3 v) = 0;
    virtual void setDirection(float3 v) = 0;
    virtual void setRange(float v) = 0;
    virtual void setSpotAngle(float v) = 0;
    virtual void setColor(float3 v) = 0;
    virtual void setIntensity(float v) = 0;
    virtual void setDisperse(float v) = 0;
    virtual void setMesh(IMeshInstance* v) = 0;

    virtual bool      isEnabled() const = 0;
    virtual LightType getType() const = 0;
    virtual float3    getPosition() const = 0;
    virtual float3    getDirection() const = 0;
    virtual float     getRange() const = 0;
    virtual float     getSpotAngle() const = 0;
    virtual float3    getColor() const = 0;
    virtual float     getIntensity() const = 0;
    virtual float     getDisperse() const = 0;
    virtual IMeshInstance* getMesh() const = 0;
};
using ILightPtr = ref_ptr<ILight>;


class ICamera : public IObject
{
public:
    virtual void setPosition(float3 v) = 0;
    virtual void setDirection(float3 dir, float3 up = float3::up()) = 0;
    virtual void setFOV(float v) = 0;
    virtual void setNear(float v) = 0;
    virtual void setFar(float v) = 0;
    virtual void setRenderTarget(IRenderTarget* v) = 0;

    virtual float3 getPosition() const = 0;
    virtual float3 getDirection() const = 0;
    virtual float getFOV() const = 0;
    virtual float getNear() const = 0;
    virtual float getFar() const = 0;
    virtual IRenderTarget* getRenderTarget() const = 0;
};
using ICameraPtr = ref_ptr<ICamera>;


class IBlendshapeFrame
{
public:
    virtual void setDeltaPoints(const float3* v, size_t n) = 0;
    virtual void setDeltaNormals(const float3* v, size_t n) = 0;
    virtual void setDeltaTangents(const float3* v, size_t n) = 0;
    virtual void setDeltaUV(const float2* v, size_t n) = 0;
    virtual Span<float3> getDeltaPoints() const = 0;
    virtual Span<float3> getDeltaNormals() const = 0;
    virtual Span<float3> getDeltaTangents() const = 0;
    virtual Span<float2> getDeltaUV() const = 0;

protected:
    virtual ~IBlendshapeFrame() {}
};

class IBlendshape
{
public:
    virtual void setName(const char* name) = 0;
    virtual int getFrameCount() const = 0;
    virtual IBlendshapeFrame* getFrame(int i) = 0;
    virtual IBlendshapeFrame* addFrame(float weight = 1.0f) = 0;
    virtual void removeFrame(IBlendshapeFrame* f) = 0;
protected:
    virtual ~IBlendshape() {}
};

class IMesh : public IObject
{
public:
    virtual void setIndices(const int* v, size_t n) = 0; // all faces must be triangles
    virtual void setPoints(const float3* v, size_t n) = 0;
    virtual void setNormals(const float3* v, size_t n) = 0;  // per-vertex
    virtual void setTangents(const float3* v, size_t n) = 0; // 
    virtual void setUV(const float2* v, size_t n) = 0;       // 
    virtual void setMaterialIDs(const int* v, size_t n) = 0; // per-face
    virtual Span<int>    getIndices() const = 0;
    virtual Span<float3> getPoints() const = 0;
    virtual Span<float3> getNormals() const = 0;
    virtual Span<float3> getTangents() const = 0;
    virtual Span<float2> getUV() const = 0;
    virtual Span<int>    getMaterialIDs() const = 0;
    virtual void markDynamic() = 0;

    virtual void setJointBindposes(const float4x4* v, size_t n) = 0;
    virtual void setJointWeights(const JointWeight* v, size_t n) = 0;
    virtual void setJointCounts(const int* v, size_t n) = 0;
    virtual Span<float4x4>    getJointBindposes() const = 0;
    virtual Span<JointWeight> getJointWeights() const = 0;
    virtual Span<int>         getJointCounts() const = 0;

    virtual int getBlendshapeCount() const = 0;
    virtual IBlendshape* getBlendshape(int i) = 0;
    virtual IBlendshape* addBlendshape() = 0;
    virtual void removeBlendshape(IBlendshape* f) = 0;
};
using IMeshPtr = ref_ptr<IMesh>;


class IMeshInstance : public IObject
{
public:
    virtual void setEnabled(bool v) = 0;
    virtual void setFlag(InstanceFlag f, bool v) = 0;
    virtual void setMaterial(IMaterial* v, int slot = 0) = 0;
    virtual void setTransform(const float4x4& v) = 0;
    virtual void setJointMatrices(const float4x4* v) = 0;
    virtual void setBlendshapeWeights(const float* v) = 0;

    virtual IMesh*          getMesh() const = 0;
    virtual bool            isEnabled() const = 0;
    virtual bool            getFlag(InstanceFlag f) const = 0;
    virtual IMaterial*      getMaterial(int slot = 0) const = 0;
    virtual float4x4        getTransform() const = 0;
    virtual Span<float4x4>  getJointMatrices() const = 0;
    virtual Span<float>     getBlendshapeWeights() const = 0;
};
using IMeshInstancePtr = ref_ptr<IMeshInstance>;


class IScene : public IObject
{
public:
    virtual void            setEnabled(bool v) = 0;
    virtual void            setBackgroundColor(float3 v) = 0;
    virtual bool            isEnabled() const = 0;
    virtual float3          getBackgroundColor() const = 0;

    virtual int             getCameraCount() const = 0;
    virtual ICamera*        getCamera(int i) const = 0;
    virtual void            addCamera(ICamera* v) = 0;
    virtual void            removeCamera(ICamera* v) = 0;

    virtual int             getLightCount() const = 0;
    virtual ILight*         getLight(int i) const = 0;
    virtual void            addLight(ILight* v) = 0;
    virtual void            removeLight(ILight* v) = 0;

    virtual int             getInstanceCount() const = 0;
    virtual IMeshInstance*  getInstance(int i) const = 0;
    virtual void            addInstance(IMeshInstance* v) = 0;
    virtual void            removeInstance(IMeshInstance* v) = 0;
};
using IScenePtr = ref_ptr<IScene>;


class IContext : public IObject
{
public:
    virtual ICameraPtr       createCamera() = 0;
    virtual ILightPtr        createLight() = 0;
    virtual IRenderTargetPtr createRenderTarget(int width, int height, Format format) = 0;
    virtual IRenderTargetPtr createRenderTarget(IWindow* window, Format format) = 0;
    virtual ITexturePtr      createTexture(int width, int height, Format format) = 0;
    virtual IMaterialPtr     createMaterial() = 0;
    virtual IMeshPtr         createMesh() = 0;
    virtual IMeshInstancePtr createMeshInstance(IMesh* v) = 0;
    virtual IScenePtr        createScene() = 0;

    virtual void render() = 0;
    virtual void finish() = 0;

    virtual void* getDevice() = 0;
    virtual const char* getDeviceName() = 0;
    virtual const char* getTimestampLog() = 0;
};
using IContextPtr = ref_ptr<IContext>;



class IWindowCallback
{
public:
    virtual ~IWindowCallback() {}
    virtual void onDestroy() {}
    virtual void onClose() {}
    virtual void onResize(int w, int h) {}
    virtual void onMinimize() {}
    virtual void onMaximize() {}
    virtual void onKeyDown(int key) {}
    virtual void onKeyUp(int key) {}
    virtual void onMouseMove(float2 pos, float2 move, int buttons) {}
    virtual void onMouseWheel(float wheel, int buttons) {}
    virtual void onMouseDown(int button) {}
    virtual void onMouseUp(int button) {}
};

class IWindow : public IObject
{
public:
    virtual void close() = 0;
    virtual void processMessages() = 0;

    virtual void addCallback(IWindowCallback* cb) = 0;
    virtual void removeCallback(IWindowCallback* cb) = 0;

    virtual bool isClosed() const = 0;
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
    virtual void* getHandle() = 0;
};
using IWindowPtr = ref_ptr<IWindow>;


} // namespace gpt

#ifdef gptStatic
    #define gptAPI 
#else
    #ifdef _WIN32
        #define gptAPI extern "C" __declspec(dllexport)
    #else
        #define gptAPI extern "C" __attribute__((visibility("default")))
    #endif
#endif

gptAPI gpt::IGlobals* gptGetGlobals();
gptAPI gpt::IContext* gptCreateContext_(gpt::DeviceType type);
gptAPI gpt::IWindow* gptCreateWindow_(int width, int height, gpt::WindowFlag flags);

inline gpt::IContextPtr gptCreateContext(gpt::DeviceType type)
{
    return gpt::IContextPtr(gptCreateContext_(type));
}

inline gpt::IWindowPtr gptCreateWindow(int width, int height, gpt::WindowFlag flags = gpt::WindowFlag::Default)
{
    return gpt::IWindowPtr(gptCreateWindow_(width, height, flags));
}
