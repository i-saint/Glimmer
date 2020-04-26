#pragma once

namespace lpt {

enum class TextureFormat : uint32_t
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
    Spot,
    Point,
    ReversePoint,
};

enum class MaterialType : uint32_t
{
    Default,
};

#ifndef lptImpl
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
#endif

struct JointWeight
{
    float weight;
    int index;
};


class IEntity
{
public:
    virtual ~IEntity() {}
    virtual int addRef() = 0;
    virtual int release() = 0;
    virtual int getRef() const = 0;

    virtual void setName(const char* name) = 0;
    virtual const char* getName() const = 0;
};


class ICamera : public IEntity
{
public:
    virtual void setPosition(float3 v) = 0;
    virtual void setDirection(float3 v, float3 up = float3::up()) = 0;
    virtual void setFOV(float v) = 0;
    virtual void setNear(float v) = 0;
    virtual void setFar(float v) = 0;
};


class ILight : public IEntity
{
public:
    virtual void setType(LightType v) = 0;
    virtual void setPosition(float3 v) = 0;
    virtual void setDirection(float3 v) = 0;
    virtual void setRange(float v) = 0;
    virtual void setSpotAngle(float v) = 0;
    virtual void setColor(float3 v) = 0;
};


class ITexture : public IEntity
{
public:
    virtual void upload(const void* src) = 0;
    virtual void* getDeviceObject() = 0;
};


class IRenderTarget : public IEntity
{
public:
    virtual void enableReadback(bool v) = 0;
    virtual void readback(void* dst) = 0;
    virtual void* getDeviceObject() = 0;
};


class IMaterial : public IEntity
{
public:
    virtual void setType(MaterialType v) = 0;
    virtual void setDiffuse(float3 v) = 0;
    virtual void setRoughness(float v) = 0;
    virtual void setEmissive(float3 v) = 0;
    virtual void setDiffuseTexture(ITexture* v) = 0;
    virtual void setEmissiveTexture(ITexture* v) = 0;
};


class IMesh : public IEntity
{
public:
    virtual void setIndices(const int* v, size_t n) = 0;
    virtual void setPoints(const float3* v, size_t n) = 0;
    virtual void setNormals(const float3* v, size_t n) = 0;
    virtual void setTangents(const float3* v, size_t n) = 0;
    virtual void setUV(const float2* v, size_t n) = 0;

    virtual void setJointBindposes(const float4x4* v, size_t n) = 0;
    virtual void setJointWeights(const JointWeight* v, size_t n) = 0;
    virtual void setJointCounts(const uint8_t* v, size_t n) = 0;

    virtual void markDynamic() = 0;
};


class IMeshInstance : public IEntity
{
public:
    virtual void setMaterial(IMaterial* v) = 0;
    virtual void setTransform(const float4x4& v) = 0;
    virtual void setJointMatrices(const float4x4* v) = 0;
};


class IScene : public IEntity
{
public:
    virtual void setRenderTarget(IRenderTarget* v) = 0;
    virtual void setCamera(ICamera* v) = 0;
    virtual void addLight(ILight* v) = 0;
    virtual void removeLight(ILight* v) = 0;
    virtual void addMesh(IMeshInstance* v) = 0;
    virtual void removeMesh(IMeshInstance* v) = 0;
    virtual void clear() = 0;
};


class IContext : public IEntity
{
public:
    virtual ICamera*        createCamera() = 0;
    virtual ILight*         createLight() = 0;
    virtual IRenderTarget*  createRenderTarget(TextureFormat format, int width, int height) = 0;
    virtual ITexture*       createTexture(TextureFormat format, int width, int height) = 0;
    virtual IMaterial*      createMaterial() = 0;
    virtual IMesh*          createMesh() = 0;
    virtual IMeshInstance*  createMeshInstance(IMesh* v) = 0;
    virtual IScene*         createScene() = 0;

    virtual void render() = 0;
    virtual void finish() = 0;

    virtual void* getDevice() = 0;
    virtual const char* getTimestampLog() = 0;
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

    T* get() { return m_ptr; }
    const T* get() const { return m_ptr; }

    T& operator*() { return *m_ptr; }
    const T& operator*() const { return *m_ptr; }
    T* operator->() { return m_ptr; }
    const T* operator->() const { return m_ptr; }
    operator T* () { return m_ptr; }
    operator const T* () const { return m_ptr; }
    operator bool() const { return m_ptr; }
    bool operator==(const ref_ptr<T>& v) const { return m_ptr == v.m_ptr; }
    bool operator!=(const ref_ptr<T>& v) const { return m_ptr != v.m_ptr; }

private:
    T* m_ptr = nullptr;
};
using ICameraPtr = ref_ptr<ICamera>;
using ILightPtr = ref_ptr<ILight>;
using ITexturePtr = ref_ptr<ITexture>;
using IRenderTargetPtr = ref_ptr<IRenderTarget>;
using IMaterialPtr = ref_ptr<IMaterial>;
using IMeshPtr = ref_ptr<IMesh>;
using IMeshInstancePtr = ref_ptr<IMeshInstance>;
using IScenePtr = ref_ptr<IScene>;
using IContextPtr = ref_ptr<IContext>;

} // namespace lpt


#ifdef _WIN32
    #define lptAPI extern "C" __declspec(dllexport)
#else
    #define lptAPI extern "C" __attribute__((visibility("default")))
#endif
lptAPI lpt::IContext* lptCreateContextDXR();

