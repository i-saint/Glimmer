#include "pch.h"
#include "Test.h"
#include "AssetGenerator.h"
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

class GlimmerTest : public gpt::IWindowCallback
{
public:
    void onMouseMove(float2 pos, float2 move, int buttons) override;
    void onMouseWheel(float wheel, int buttons) override;
    void onKeyDown(int key) override;

    bool init();
    void messageLoop();

private:
    gpt::IContextPtr m_ctx;
    gpt::IWindowPtr m_window;

    gpt::IScenePtr m_scene;
    gpt::IRenderTargetPtr m_render_target;
    gpt::ICameraPtr m_camera;
    gpt::ILightPtr m_directional_light;
    gpt::ILightPtr m_point_light;

    gpt::IMaterialPtr m_mat_checker;
    gpt::IMaterialPtr m_mat_diffuse;
    gpt::IMaterialPtr m_mat_reflective;
    gpt::IMaterialPtr m_mat_transparent;
    gpt::IMaterialPtr m_mat_emissive;

    gpt::IMeshPtr m_mesh_floor;
    gpt::IMeshPtr m_mesh_triangle;
    gpt::IMeshPtr m_mesh_deformable;
    gpt::IMeshPtr m_mesh_cube;
    gpt::IMeshPtr m_mesh_ico;
    gpt::IMeshPtr m_mesh_sphere;

    gpt::IMeshInstancePtr m_inst_triangle;
    gpt::IMeshInstancePtr m_inst_deformable;

    int m_frame = 0;
    float3 m_camera_position;
    float3 m_camera_target;
};


void GlimmerTest::onMouseMove(float2 pos, float2 move, int buttons)
{
    // handle mouse drag
    if ((buttons & 0x2) != 0) {
        // rotate
        float3 axis = mu::cross(mu::normalize(m_camera_target - m_camera_position), float3::up());
        m_camera_position = mu::to_mat3x3(mu::rotate_y(-move.x * mu::DegToRad * 0.1f)) * m_camera_position;
        m_camera_position = mu::to_mat3x3(mu::rotate(axis, move.y * mu::DegToRad * 0.1f)) * m_camera_position;
        m_camera->setPosition(m_camera_position);
        m_camera->setDirection(mu::normalize(m_camera_target - m_camera_position));

        //// zoom
        //float s = (move.x + move.y) * 0.002f + 1.0f;
        //float d = mu::length(m_camera_target - m_camera_position) * s;
        //float3 dir = mu::normalize(m_camera_position - m_camera_target);
        //m_camera_position = m_camera_target + (dir * d);
        //m_camera->setPosition(m_camera_position);
    }
    else if ((buttons & 0x4) != 0) {
        // move
        float len = mu::length(m_camera_target - m_camera_position);
        float3 t = float3{ -move.x, move.y, 0.0f } *0.001f * len;

        float3 dir = mu::normalize(m_camera_target - m_camera_position);
        quatf rot = mu::look_quat(dir, float3::up());
        t = to_mat3x3(rot) * t;

        m_camera_position += t;
        m_camera_target += t;
        m_camera->setPosition(m_camera_position);
    }
}

void GlimmerTest::onMouseWheel(float wheel, int buttons)
{
    {
        // zoom
        float s = -wheel * 0.1f + 1.0f;
        float d = mu::length(m_camera_target - m_camera_position) * s;
        float3 dir = mu::normalize(m_camera_position - m_camera_target);
        m_camera_position = m_camera_target + (dir * d);
        m_camera->setPosition(m_camera_position);
    }
}

void GlimmerTest::onKeyDown(int key)
{
}


bool GlimmerTest::init()
{
    gptGetGlobals()->enableTimestamp(true);
    gptGetGlobals()->enableStrictUpdateCheck(true);

    m_ctx = gptCreateContext(gpt::DeviceType::DXR);
    if (!m_ctx) {
        printf("DXR is not supported on this system.\n");
        return false;
    }
    printf("device: %s\n", m_ctx->getDeviceName());

    const int rt_width = 1280;
    const int rt_height = 720;
#ifdef EnableWindow
    const auto rt_format = gpt::Format::RGBAu8;
    m_window = gptCreateWindow(rt_width, rt_height);
    m_window->addCallback(this);
    m_render_target = m_ctx->createRenderTarget(m_window, rt_format);
#else
    const auto rt_format = gpt::Format::RGBAf32;
    m_render_target = m_ctx->createRenderTarget(rt_width, rt_height, rt_format);
#endif

    m_scene = m_ctx->createScene();
    m_camera = m_ctx->createCamera();
    m_directional_light = m_ctx->createLight();
    m_point_light = m_ctx->createLight();

    m_render_target->enableReadback(true);
    m_camera->setRenderTarget(m_render_target);

    m_scene->setBackgroundColor(float3{0.01f, 0.01f, 0.01f });
    m_scene->addCamera(m_camera);
    m_scene->addLight(m_directional_light);
    m_scene->addLight(m_point_light);

    auto checker_texture = m_ctx->createTexture(512, 512, gpt::Format::RGBAf16);
    auto dot_texture = m_ctx->createTexture(512, 512, gpt::Format::RGBAf16);
    auto dot_normal_texture = m_ctx->createTexture(512, 512, gpt::Format::RGBAf16);
    m_mat_checker = m_ctx->createMaterial();
    m_mat_diffuse = m_ctx->createMaterial();
    m_mat_reflective = m_ctx->createMaterial();
    m_mat_transparent = m_ctx->createMaterial();
    m_mat_emissive = m_ctx->createMaterial();


    {
        RawVector<half4> image;
        image.resize(checker_texture->getWidth() * checker_texture->getHeight());
        GenerateCheckerImage(image.data(), checker_texture->getWidth(), checker_texture->getHeight(), 32);
        checker_texture->upload(image.cdata());
    }
    {
        RawVector<half4> height;
        RawVector<half4> normals;
        height.resize(checker_texture->getWidth() * checker_texture->getHeight());
        normals.resize(checker_texture->getWidth() * checker_texture->getHeight());
        GeneratePolkaDotImage(height.data(), checker_texture->getWidth(), checker_texture->getHeight(), 32);
        GenerateNormalMapFromHeightMap(normals.data(), height.data(), checker_texture->getWidth(), checker_texture->getHeight());
        dot_texture->upload(height.cdata());
        dot_normal_texture->upload(normals.cdata());
    }

    m_mat_checker->setDiffuse(float3{ 0.9f, 0.9f, 0.9f });
    m_mat_checker->setRoughness(0.8f);
    //m_mat_checker->setRoughnessMap(checker_texture);
    m_mat_checker->setDiffuseMap(checker_texture);
    //m_mat_checker->setDiffuseMap(dot_texture);
    m_mat_checker->setNormalMap(dot_normal_texture);
    //m_mat_checker->setEmissiveMap(dot_normal_texture);

    m_mat_diffuse->setDiffuse(float3{ 0.7f, 0.7f, 0.7f });
    m_mat_diffuse->setRoughness(0.5f);

    m_mat_reflective->setDiffuse(float3{ 0.5f, 0.5f, 0.7f });
    m_mat_reflective->setRoughness(0.02f);

    m_mat_transparent->setType(gpt::MaterialType::Transparent);
    m_mat_transparent->setDiffuse(float3{ 0.6f, 0.6f, 0.7f });
    m_mat_transparent->setRoughness(0.0f);
    m_mat_transparent->setOpacity(0.5f);
    m_mat_transparent->setRefractionIndex(1.5f); // https://en.wikipedia.org/wiki/List_of_refractive_indices

    m_mat_emissive->setDiffuse(float3{ 0.8f, 0.8f, 0.8f });
    m_mat_emissive->setEmissive(float3{ 0.9f, 0.1f, 0.2f });
    m_mat_emissive->setEmissiveRange(100.0f);
    //m_mat_emissive->setEmissiveSampleCount(4);

    // camera
    {
        m_camera_position = { 0.0f, 2.0f, -8.0f };
        m_camera_target = { 0.0f, 0.0f, 0.0f };
        m_camera->setPosition(m_camera_position);
        m_camera->setDirection(mu::normalize(m_camera_target - m_camera_position));
    }

    // lights
    {
        float3 pos{ 2.0f, 7.0f, -8.0f };
        float3 target{ 0.0f, 0.0f, 0.0f };
        float3 color{ 0.95f, 0.925f, 1.0f };
        m_directional_light->setType(gpt::LightType::Directional);
        m_directional_light->setDirection(mu::normalize(target - pos));
        m_directional_light->setColor(color);
        m_directional_light->setIntensity(0.5f);
        m_directional_light->setDisperse(0.2f);
        m_directional_light->setEnabled(false);
    }
    {
        float3 pos{ 2.0f, 7.0f, 4.0f };
        float3 target{ 0.0f, 0.0f, 0.0f };
        float3 color{ 0.95f, 0.925f, 1.0f };
        m_point_light->setType(gpt::LightType::Point);
        m_point_light->setPosition(pos);
        m_point_light->setDirection(mu::normalize(target - pos));
        m_point_light->setColor(color);
        m_point_light->setIntensity(0.5f);
        m_point_light->setDisperse(0.1f);
        //m_point_light->setEnabled(false);
    }

    // create meshes
    {
        // floor quad
        static const float3 points[]{
            {-5.0f, 0.0f, 5.0f},
            { 5.0f, 0.0f, 5.0f},
            { 5.0f, 0.0f,-5.0f},
            {-5.0f, 0.0f,-5.0f},
        };
        static const float2 uv[]{
            { 0.0f, 1.0f},
            { 1.0f, 1.0f},
            { 1.0f, 0.0f},
            { 0.0f, 0.0f},
        };
        static const int indices[]{
            0, 1, 2, 0, 2, 3,
        };
        m_mesh_floor = m_ctx->createMesh();
        m_mesh_floor->setPoints(points, _countof(points));
        m_mesh_floor->setUV(uv, _countof(uv));
        m_mesh_floor->setIndices(indices, _countof(indices));

        // add a instance with default transform
        auto inst = m_ctx->createMeshInstance(m_mesh_floor);
        inst->setMaterial(m_mat_checker);
        m_scene->addInstance(inst);
    }

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

        m_mesh_triangle = m_ctx->createMesh();
        m_mesh_triangle->setName("Triangle");
        m_mesh_triangle->setPoints(points, _countof(points));
        m_mesh_triangle->setIndices(indices, _countof(indices));

        //m_inst_triangle = m_ctx->createMeshInstance(m_mesh_triangle);
        //m_inst_triangle->setMaterial(m_mat_reflective);
        //m_scene->addMesh(m_inst_triangle);
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
            {0.0f, 0.5f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 1.5f, 0.0f},
        };

        m_mesh_deformable = m_ctx->createMesh();
        m_mesh_deformable->setName("Deformable Triangle");
        m_mesh_deformable->setPoints(points, _countof(points));
        m_mesh_deformable->setIndices(indices, _countof(indices));
        {
            // joints
            m_mesh_deformable->setJointCounts(joint_counts, _countof(joint_counts));
            m_mesh_deformable->setJointWeights(joint_weights, _countof(joint_weights));
            m_mesh_deformable->setJointBindposes(joint_bindposes, _countof(joint_bindposes));
        }
        {
            // bllendshapes
            auto bs = m_mesh_deformable->addBlendshape();
            auto frame = bs->addFrame();
            frame->setDeltaPoints(delta_points, _countof(delta_points));
        }

        static const float4x4 joint_matrices[]{
            mu::translate(float3{0.0f, 0.0f, 0.5f}),
            mu::translate(float3{0.0f, 0.0f, 1.0f}),
            mu::translate(float3{0.0f, 0.0f, 1.5f}),
        };
        static const float bs_weights[]{
            1.0f,
        };

        //m_inst_deformable = m_ctx->createMeshInstance(m_mesh_deformable);
        //m_inst_deformable->setMaterial(m_mat_emissive);
        //m_inst_deformable->setJointMatrices(joint_matrices);
        //m_inst_deformable->setBlendshapeWeights(bs_weights);
        //m_scene->addMesh(m_inst_deformable);
    }

    {
        RawVector<int> indices;
        RawVector<float3> points;
        RawVector<float3> normals;
        RawVector<float2> uv;
        GenerateCubeMesh(indices, points, normals, uv, 0.5f);

        m_mesh_cube = m_ctx->createMesh();
        m_mesh_cube->setName("Cube");
        m_mesh_cube->setIndices(indices.data(), indices.size());
        m_mesh_cube->setPoints(points.data(), points.size());
        m_mesh_cube->setNormals(normals.data(), normals.size());
        m_mesh_cube->setUV(uv.data(), uv.size());

        auto inst = m_ctx->createMeshInstance(m_mesh_cube);
        inst->setTransform(mu::transform(float3{2.0f, 0.5f, 0.0f}, quatf::identity()));
        inst->setMaterial(m_mat_transparent);
        m_scene->addInstance(inst);

        inst = m_ctx->createMeshInstance(m_mesh_cube);
        inst->setTransform(mu::transform(float3{ 0.8f, 1.5f, 2.0f }, mu::rotate_y(30.0f * mu::DegToRad), float3{ 1.0f, 3.0f, 1.0f }));
        inst->setMaterial(m_mat_diffuse);
        m_scene->addInstance(inst);

        inst = m_ctx->createMeshInstance(m_mesh_cube);
        inst->setTransform(mu::transform(float3{ -1.2f, 1.0f, 2.0f }, mu::rotate_y(45.0f * mu::DegToRad), float3{ 1.0f, 2.0f, 1.0f }));
        inst->setMaterial(m_mat_reflective);
        m_scene->addInstance(inst);
    }

    {
        RawVector<int> indices;
        RawVector<float3> points, points_expanded;
        RawVector<float3> normals;
        RawVector<float2> uv;
        GenerateIcoSphereMesh(indices, points, normals, 0.5f, 0);

        size_t ni = indices.size();
        points_expanded.resize_discard(ni);
        for (size_t i = 0; i < ni; ++i)
            points_expanded[i] = points[indices[i]];
        std::iota(indices.begin(), indices.end(), 0);

        m_mesh_ico = m_ctx->createMesh();
        m_mesh_ico->setName("Ico");
        m_mesh_ico->setIndices(indices.data(), indices.size());
        m_mesh_ico->setPoints(points_expanded.data(), points_expanded.size());

        auto inst = m_ctx->createMeshInstance(m_mesh_ico);
        inst->setTransform(mu::transform(float3{ -1.7f, 0.5f, -1.5f }, quatf::identity()));
        inst->setMaterial(m_mat_reflective);
        m_scene->addInstance(inst);

        inst = m_ctx->createMeshInstance(m_mesh_ico);
        inst->setTransform(mu::transform(float3{ -0.2f, 0.5f, -2.0f }, quatf::identity()));
        //inst->setMaterial(m_mat_diffuse);
        inst->setMaterial(m_mat_transparent);
        //inst->setFlag(gpt::InstanceFlag::Visible, false);
        //inst->setFlag(gpt::InstanceFlag::CastShadows, true);
        m_scene->addInstance(inst);
    }

    {
        RawVector<int> indices;
        RawVector<float3> points;
        RawVector<float3> normals;
        RawVector<float2> uv;
        GenerateIcoSphereMesh(indices, points, normals, 0.5f, 3);

        m_mesh_sphere = m_ctx->createMesh();
        m_mesh_sphere->setName("Sphere");
        m_mesh_sphere->setIndices(indices.data(), indices.size());
        m_mesh_sphere->setPoints(points.data(), points.size());
        m_mesh_sphere->setNormals(normals.data(), normals.size());
        m_mesh_sphere->setUV(uv.data(), uv.size());

        auto inst = m_ctx->createMeshInstance(m_mesh_sphere);
        inst->setTransform(mu::transform(float3{ -2.0f, 0.5f, 0.0f }, quatf::identity()));
        inst->setMaterial(m_mat_transparent);
        m_scene->addInstance(inst);

        inst = m_ctx->createMeshInstance(m_mesh_sphere);
        inst->setTransform(mu::transform(float3{ 0.0f, 0.8f, 0.0f }, quatf::identity()));
        inst->setMaterial(m_mat_emissive);
        //inst->setFlag(gpt::InstanceFlag::Visible, false);
        inst->setFlag(gpt::InstanceFlag::LightSource, true);
        m_scene->addInstance(inst);
    }

    return true;
}

void GlimmerTest::messageLoop()
{
#ifdef EnableWindow
    {
        while (!m_window->isClosed()) {
            m_window->processMessages();
            m_ctx->render();
            m_ctx->finish();

            //float bs_weights[]{
            //    sin((float)m_frame * 0.001f) * 0.5f + 0.5f,
            //};
            //m_inst_deformable->setBlendshapeWeights(bs_weights);
            //m_inst_triangle->setTransform(mu::to_mat4x4(mu::rotate_y(-(float)m_frame * 0.002f)));
            //m_directional_light->setEnabled(m_frame % 240 < 120);

            float s = std::sin((float)m_frame * mu::DegToRad * 0.4f) * 0.5f + 0.5f;
            m_mat_emissive->setEmissive(float3{ 0.4f, 0.4f, 1.0f } * (1.5f * s));

            //m_mat_transparent->setRefractionIndex(0.5f + s);

            printf("%s\n", m_ctx->getTimestampLog());
            ++m_frame;
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

TestCase(TestMinimum)
{
    GlimmerTest test;
    if (test.init()) {
        test.messageLoop();
    }
}

