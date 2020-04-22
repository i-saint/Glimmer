#pragma once
#include "Foundation/lptRefPtr.h"
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

struct JointWeight
{
    float weight;
    int index;
};


class IObject
{
public:
    virtual ~IObject() {}
    virtual int addRef() = 0;
    virtual int release() = 0;

    virtual void setName(const char* name) = 0;
};

class ICamera : public IObject
{
public:
    virtual void setPosition(float3 v) = 0;
    virtual void setRotation(quatf v) = 0;
    virtual void setFOV(float v) = 0;
    virtual void setNear(float v) = 0;
    virtual void setFar(float v) = 0;
};

class ILight : public IObject
{
public:
    virtual void setType(LightType v) = 0;
    virtual void setPosition(float3 v) = 0;
    virtual void setDirection(float3 v) = 0;
    virtual void setRange(float v) = 0;
    virtual void setSpotAngle(float v) = 0;
    virtual void setColor(float3 v) = 0;
};

class ITexture : public IObject
{
public:
    virtual void setup(TextureFormat format, int width, int height) = 0;
    virtual bool upload(const void* src) = 0;
};

class IRenderTarget : public IObject
{
public:
    virtual void setup(TextureFormat format, int width, int height) = 0;
    virtual bool readback(void* dst) = 0;
};

class IMaterial : public IObject
{
public:
    virtual void setDiffuse(float3 v) = 0;
    virtual void setRoughness(float v) = 0;
    virtual void setEmission(float3 v) = 0;

    virtual void setDiffuseTexture(ITexture* v) = 0;
    virtual void setEmissionTexture(ITexture* v) = 0;
};

class IMesh : public IObject
{
public:
    virtual void setIndices(const int* v, size_t n) = 0;
    virtual void setPoints(const float3* v, size_t n) = 0;
    virtual void setNormals(const float3* v, size_t n) = 0;
    virtual void setTangents(const float3* v, size_t n) = 0;
    virtual void setUV(const float2* v, size_t n) = 0;

    virtual void setJointBindposes(const float4x4* v, size_t n) = 0;
    virtual void setJointWeights(const uint8_t* counts, size_t ncounts, const JointWeight* weights, size_t nweights) = 0;
};

class IMeshInstance : public IObject
{
public:
    virtual void setTransform(const float4x4& v) = 0;
    virtual void setJointMatrices(const float4x4* v) = 0;
    virtual void setMaterial(IMaterial* v) = 0;
};

class IScene : public IObject
{
public:
    virtual void setRenderTarget(IRenderTarget* v) = 0;
    virtual void setCamera(ICamera* v) = 0;
    virtual void addLight(ILight* v) = 0;
    virtual void addMesh(IMeshInstance* v) = 0;
    virtual void clear() = 0;
};


class IContext : public IObject
{
public:
    virtual ICamera* createCamera() = 0;
    virtual ILight* createLight() = 0;
    virtual ITexture* createTexture() = 0;
    virtual IMaterial* createMaterial() = 0;
    virtual IMesh* createMesh() = 0;
    virtual IMeshInstance* createMeshInstance() = 0;
    virtual IScene* createScene() = 0;

    virtual void renderStart(IScene* v) = 0;
    virtual void renderFinish(IScene* v) = 0;
};


IContext* CreateContextDXR();

} // namespace lpt
