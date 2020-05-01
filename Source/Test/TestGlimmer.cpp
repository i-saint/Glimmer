#include "pch.h"
#include "Test.h"
#include "MeshGenerator.h"
#define gptImpl
#include "gptInterface.h"

TestCase(TestMath)
{
    {
        mu::ONBf onb(float3::up());
        float3 p = mu::cosine_sample_hemisphere(0.5f, 0.5f);
        float3 r = onb.inverse_transform(p);
        printf("p: {%f, %f, %f}\n", p.x, p.y, p.z);
        printf("r: {%f, %f, %f}\n", r.x, r.y, r.z);
    }
    {
        auto dir = mu::normalize(float3{ 1.0f, -1.0f, -1.0f });
        auto rot = mu::look_quat(dir);
        auto mat = mu::to_mat3x3(rot);
        auto dir2 = mat * float3{ 0.0f, 0.0f, 1.0f };
        printf("dir: {%f, %f, %f}\n", dir.x, dir.y, dir.z);
        printf("dir2: {%f, %f, %f}\n", dir2.x, dir2.y, dir2.z);
    }
}

#define EnableWindow

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
#ifdef EnableWindow
    const auto rt_format = gpt::Format::RGBAu8;
    auto window = gptCreateWindow(rt_width, rt_height);
    auto render_target = ctx->createRenderTarget(window, rt_format);
#else
    const auto rt_format = gpt::Format::RGBAf32;
    auto render_target = ctx->createRenderTarget(rt_width, rt_height, rt_format);
#endif

    auto scene = ctx->createScene();
    auto camera = ctx->createCamera();
    auto light = ctx->createLight();
    auto material1 = ctx->createMaterial();
    auto material2 = ctx->createMaterial();

    render_target->enableReadback(true);
    camera->setRenderTarget(render_target);
    scene->setCamera(camera);
    scene->addLight(light);

    material1->setDiffuse(float3{ 1.0f, 1.0f, 1.0f });
    material2->setDiffuse(float3{ 0.5f, 0.5f, 0.5f });
    {
        float3 pos{ 0.0f, 2.0f, -8.0f };
        float3 target{ 0.0f, 0.0f, 0.0f };
        camera->setPosition(pos);
        camera->setDirection(mu::normalize(target - pos));
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

        auto triangle = ctx->createMesh();
        triangle->setName("Triangle");
        triangle->setPoints(points, _countof(points));
        triangle->setIndices(indices, _countof(indices));

        // add a instance with default transform (identity matrix)
        auto instance = ctx->createMeshInstance(triangle);
        instance->setMaterial(material1);
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
            {0.0f, -1.0f, 0.0f},
            {0.0f, -2.0f, 0.0f},
            {0.0f, -3.0f, 0.0f},
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
            auto frame = bs->addFrame();
            frame->setDeltaPoints(delta_points, _countof(delta_points));
        }

        static const float4x4 joint_matrices[]{
            mu::translate(float3{0.0f, 0.0f, -1.0f}),
            mu::translate(float3{0.0f, 0.0f, -2.0f}),
            mu::translate(float3{0.0f, 0.0f, -3.0f}),
        };
        static const float bs_weights[]{
            1.0f,
        };

        auto instance = ctx->createMeshInstance(triangle);
        instance->setMaterial(material1);
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
        instance->setMaterial(material2);
        scene->addMesh(instance);
    }

    // render!
#ifdef EnableWindow
    {
        int frame = 0;
        while (!window->isClosed()) {
            window->processMessages();
            ctx->render();
            ctx->finish();

            float3 pos{ 0.0f, 2.0f, -8.0f };
            float3 target{ 0.0f, 0.0f, 0.0f };
            pos = mu::to_mat3x3(mu::rotate_y((float)frame * 0.001f)) * pos;
            pos.y += sin((float)frame * 0.002f) * 1.0f;
            camera->setPosition(pos);
            camera->setDirection(mu::normalize(target - pos));

            printf("%s\n", ctx->getTimestampLog());
            ++frame;
        }
    }
#else
    {
        ctx->render();
        ctx->finish();

        RawVector<float4> readback_buffer;
        readback_buffer.resize(rt_width * rt_height, mu::nan<float4>());
        render_target->readback(readback_buffer.data());
        printf("%s\n", ctx->getTimestampLog());
    }
#endif

}

