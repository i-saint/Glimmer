#pragma once
#include "Foundation/lptMath.h"
#include "Foundation/lptHalf.h"

namespace lpt {

enum class TextureFormat : uint32_t
{
    Unknown = 0,
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
    Unknown = 0,
    Directional = 1,
    Spot = 2,
    Point = 3,
    ReversePoint = 4,
};

#ifndef lptImpl
struct float2 { float x, y; };
struct float3 { float x, y, z; };
struct float4 { float x, y, z, w; };
using quatf = float4;
struct float4x4 { float v[4][4]; };
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
    virtual void setRotation(quatf v) = 0;
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
    virtual void setup(TextureFormat format, int width, int height) = 0;
    virtual void upload(const void* src) = 0;
    virtual void* getDeviceObject() = 0;
};


class IRenderTarget : public IEntity
{
public:
    virtual void setup(TextureFormat format, int width, int height) = 0;
    virtual void readback(void* dst) = 0;
    virtual void* getDeviceObject() = 0;
};


class IMaterial : public IEntity
{
public:
    virtual void setDiffuse(float3 v) = 0;
    virtual void setRoughness(float v) = 0;
    virtual void setEmissive(float3 v) = 0;

    virtual void setDiffuseTexture(ITexture* v) = 0;
    virtual void setEmissionTexture(ITexture* v) = 0;
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
};


class IMeshInstance : public IEntity
{
public:
    virtual void setMesh(IMesh* v) = 0;
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
    virtual void addMesh(IMeshInstance* v) = 0;
    virtual void clear() = 0;
};


class IContext : public IEntity
{
public:
    virtual ICamera*        createCamera() = 0;
    virtual ILight*         createLight() = 0;
    virtual IRenderTarget*  createRenderTarget() = 0;
    virtual ITexture*       createTexture() = 0;
    virtual IMaterial*      createMaterial() = 0;
    virtual IMesh*          createMesh() = 0;
    virtual IMeshInstance*  createMeshInstance() = 0;
    virtual IScene*         createScene() = 0;

    virtual void renderStart(IScene* v) = 0;
    virtual void renderFinish(IScene* v) = 0;

    virtual void* getDevice() = 0;
};


template<class T>
class ref_ptr
{
public:
    ref_ptr() {}
    ref_ptr(T* data) { reset(data); }
    ref_ptr(T&& data) { swap(data); }
    ref_ptr(const ref_ptr& v) { reset(v.m_ptr); }
    ref_ptr& operator=(const ref_ptr& v) { reset(v.m_ptr); return *this; }
    ~ref_ptr() { reset(); }

    void reset(T* data = nullptr)
    {
        if (m_ptr)
            m_ptr->release();
        m_ptr = data;
        if (m_ptr)
            m_ptr->addRef();
    }

    void swap(ref_ptr& v)
    {
        std::swap(m_ptr, v->m_data);
    }

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

