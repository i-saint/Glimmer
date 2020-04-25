#include "pch.h"
#include "Test.h"
#include "MeshGenerator.h"
#include "lptInterface.h"


TestCase(TestMinimum)
{
    lpt::IContextPtr ctx = lptCreateContextDXR();

    // scene
    lpt::IScenePtr scene = ctx->createScene();

    // render target
    {
        const int rt_width = 1024;
        const int rt_height = 1024;
        const lpt::TextureFormat rt_format = lpt::TextureFormat::Rf32;
        lpt::IRenderTargetPtr render_target = ctx->createRenderTarget(rt_format, rt_width, rt_height);
        scene->setRenderTarget(render_target);
    }

    // camera
    {
        lpt::ICameraPtr camera = ctx->createCamera();
        scene->setCamera(camera);
    }

    // light
    {
        lpt::ILightPtr light = ctx->createLight();
        scene->addLight(light);
    }

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
        auto *triangle = ctx->createMesh();
        triangle->setPoints((const lpt::float3*)points, _countof(points));
        triangle->setIndices(indices, _countof(indices));

        // add a instance with default transform (identity matrix)
        auto instance = ctx->createMeshInstance(triangle);
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
        auto *quad = ctx->createMesh();
        quad->setPoints((const lpt::float3*)points, _countof(points));
        quad->setIndices(indices, _countof(indices));

        // add a instance with default transform
        auto instance = ctx->createMeshInstance(quad);
        scene->addMesh(instance);
    }

    // render!
    ctx->render();
    ctx->finish();

    printf("%s\n", ctx->getTimestampLog());
}

