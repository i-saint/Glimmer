#include "pch.h"
#include "Test.h"
#include "MeshGenerator.h"
#include "lptInterface.h"


TestCase(TestMinimum)
{
    const int rt_width = 1024;
    const int rt_height = 1024;
    const lpt::TextureFormat rt_format = lpt::TextureFormat::RGBAf32;

    auto ctx = lptCreateContextDXR();
    auto scene = ctx->createScene();
    auto camera = ctx->createCamera();
    auto light = ctx->createLight();
    auto material = ctx->createMaterial();
    auto render_target = ctx->createRenderTarget(rt_format, rt_width, rt_height);

    render_target->enableReadback(true);
    scene->setRenderTarget(render_target);
    scene->setCamera(camera);
    scene->addLight(light);

    // create meshes
    {
        // standing triangle
        static const float3 points[]{
            {-1.0f, 0.0f, 0.0f},
            { 1.0f, 0.0f, 0.0f},
            { 0.0f, 2.0f, 0.0f},
        };
        static const int indices[]{
            0, 1, 2,
        };
        auto triangle = ctx->createMesh();
        triangle->setPoints((const lpt::float3*)points, _countof(points));
        triangle->setIndices(indices, _countof(indices));

        // add a instance with default transform (identity matrix)
        auto instance = ctx->createMeshInstance(triangle);
        instance->setMaterial(material);
        scene->addMesh(instance);
    }
    {
        // floor quad
        static const float3 points[]{
            {-5.0f, 0.0f, 5.0f},
            { 5.0f, 0.0f, 5.0f},
            { 5.0f, 0.0f,-5.0f},
            {-5.0f, 0.0f,-5.0f},
        };
        static const int indices[]{
            0, 1, 2, 0, 2, 3,
        };
        auto quad = ctx->createMesh();
        quad->setPoints((const lpt::float3*)points, _countof(points));
        quad->setIndices(indices, _countof(indices));

        // add a instance with default transform
        auto instance = ctx->createMeshInstance(quad);
        instance->setMaterial(material);
        scene->addMesh(instance);
    }

    // render!
    for (int i = 0; i < 10;++i) {
        ctx->render();
        ctx->finish();
        printf("%s\n", ctx->getTimestampLog());
    }

    RawVector<float4> readback_buffer;
    readback_buffer.resize(rt_width * rt_height, mu::nan<float4>());
    render_target->readback(readback_buffer.data());
}

