#include "pch.h"
#include "Test.h"
#include "MeshGenerator.h"
#define gptImpl
#include "gptInterface.h"

TestCase(TestMath)
{
    mu::ONBf onb(float3::up());
    float3 p = mu::cosine_sample_hemisphere(0.5f, 0.5f);
    float3 r = onb.inverse_transform(p);
    printf("p: {%f, %f, %f}\n", p.x, p.y, p.z);
    printf("r: {%f, %f, %f}\n", r.x, r.y, r.z);
}

TestCase(TestMinimum)
{
    gptGetGlobals()->enableTimestamp(true);
    gptGetGlobals()->enableStrictUpdateCheck(true);

    auto ctx = gptCreateContext(gpt::DeviceType::DXR);
    if (!ctx) {
        printf("DXR is not supported on this system.\n");
        return;
    }
    printf("device: %s\n", ctx->getDeviceName());

    const int rt_width = 1024;
    const int rt_height = 1024;
    //const auto rt_format = gpt::Format::RGBAf32;
    const auto rt_format = gpt::Format::RGBAu8;
    auto window = gptCreateWindow(rt_width, rt_height);

    auto scene = ctx->createScene();
    auto camera = ctx->createCamera();
    auto light = ctx->createLight();
    auto material = ctx->createMaterial();
    auto render_target = ctx->createRenderTarget(window, rt_format);

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
        triangle->setName("Triangle");
        triangle->setPoints(points, _countof(points));
        triangle->setIndices(indices, _countof(indices));

        // add a instance with default transform (identity matrix)
        auto instance = ctx->createMeshInstance(triangle);
        instance->setMaterial(material);
        scene->addMesh(instance);
    }
    {
        // deformable triangle
        static const float3 points[]{
            {-1.0f, 0.0f, 0.0f},
            { 1.0f, 0.0f, 0.0f},
            { 0.0f, 2.0f, 0.0f},
        };
        static const int indices[]{
            0, 1, 2,
        };

        static const int joint_counts[]{
            1, 1, 1,
        };
        static const gpt::JointWeight joint_weights[]{
            {1.0f, 0},
            {1.0f, 1},
            {1.0f, 2},
        };
        static const float4x4 joint_bindposes[]{
            float4x4::identity(),
            float4x4::identity(),
            float4x4::identity(),
        };

        static const float3 delta_points[]{
            {-10.0f, 0.0f, 0.0f},
            {-20.0f, 0.0f, 0.0f},
            {-30.0f, 0.0f, 0.0f},
        };

        auto triangle = ctx->createMesh();
        triangle->setName("Deformable Triangle");
        triangle->setPoints(points, _countof(points));
        triangle->setIndices(indices, _countof(indices));
        {
            // joints
            triangle->setJointCounts(joint_counts, _countof(joint_counts));
            triangle->setJointWeights(joint_weights, _countof(joint_weights));
            triangle->setJointBindposes(joint_bindposes, _countof(joint_bindposes));
        }
        {
            // bllendshapes
            auto bs = triangle->addBlendshape();
            int f = bs->addFrame();
            bs->setDeltaPoints(f, delta_points, _countof(delta_points));
        }

        static const float4x4 joint_matrices[]{
            mu::translate(float3{0.0f, 0.0f, -10.0f}),
            mu::translate(float3{0.0f, 0.0f, -20.0f}),
            mu::translate(float3{0.0f, 0.0f, -30.0f}),
        };
        static const float bs_weights[]{
            1.0f,
        };

        auto instance = ctx->createMeshInstance(triangle);
        instance->setMaterial(material);
        instance->setJointMatrices(joint_matrices);
        instance->setBlendshapeWeights(bs_weights);
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
        quad->setPoints(points, _countof(points));
        quad->setIndices(indices, _countof(indices));

        // add a instance with default transform
        auto instance = ctx->createMeshInstance(quad);
        instance->setMaterial(material);
        scene->addMesh(instance);
    }

    // render!
    RawVector<float4> readback_buffer;
    while (!window->isClosed()) {
        window->processMessages();

        ctx->render();
        ctx->finish();

        readback_buffer.resize(window->getWidth() * window->getHeight(), mu::nan<float4>());
        render_target->readback(readback_buffer.data());
        printf("%s\n", ctx->getTimestampLog());
    }
}

