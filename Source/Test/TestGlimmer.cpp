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
    void onMouseMove(int x, int y, int button) override;
    void onMouseWheel(float wheel, int buttons) override;

    bool init();
    void messageLoop();

private:
    gpt::IContextPtr m_ctx;
    gpt::IWindowPtr m_window;

    gpt::IScenePtr m_scene;
    gpt::IRenderTargetPtr m_render_target;
    gpt::ICameraPtr m_camera;
    gpt::ILightPtr m_light;

    gpt::IMaterialPtr m_mat_reflective;
    gpt::IMaterialPtr m_mat_emissive;
    gpt::IMaterialPtr m_mat_checker;

    gpt::IMeshPtr m_mesh_floor;
    gpt::IMeshPtr m_mesh_triangle;
    gpt::IMeshPtr m_mesh_deformable;
    gpt::IMeshPtr m_mesh_cube;
    gpt::IMeshPtr m_mesh_sphere;

    gpt::IMeshInstancePtr m_inst_floor;
    gpt::IMeshInstancePtr m_inst_triangle;
    gpt::IMeshInstancePtr m_inst_deformable;
    gpt::IMeshInstancePtr m_inst_cube;
    gpt::IMeshInstancePtr m_inst_sphere;

    int2 m_mouse_pos{};
    int2 m_mouse_move{};

    float3 m_camera_position;
    float3 m_camera_target;
};


void GlimmerTest::onMouseMove(int x, int y, int button)
{
    int2 new_pos{ x,y };
    m_mouse_move = new_pos - m_mouse_pos;
    m_mouse_pos = new_pos;

    // handle mouse drag
    if ((button & 0x2) != 0) {
        // rotate
        float3 axis = mu::cross(mu::normalize(m_camera_target - m_camera_position), float3::up());
        m_camera_position = mu::to_mat3x3(mu::rotate_y((float)-m_mouse_move.x * mu::DegToRad * 0.1f)) * m_camera_position;
        m_camera_position = mu::to_mat3x3(mu::rotate(axis, (float)m_mouse_move.y * mu::DegToRad * 0.1f)) * m_camera_position;
        m_camera->setPosition(m_camera_position);
        m_camera->setDirection(mu::normalize(m_camera_target - m_camera_position));

        //// zoom
        //float s = float(m_mouse_move.x + m_mouse_move.y) * 0.002f + 1.0f;
        //float d = mu::length(m_camera_target - m_camera_position) * s;
        //float3 dir = mu::normalize(m_camera_position - m_camera_target);
        //m_camera_position = m_camera_target + (dir * d);
        //m_camera->setPosition(m_camera_position);
    }
    else if ((button & 0x4) != 0) {
        // move
        float len = mu::length(m_camera_target - m_camera_position);
        float3 move = float3{ (float)-m_mouse_move.x, (float)m_mouse_move.y, 0.0f } *0.001f * len;

        float3 dir = mu::normalize(m_camera_target - m_camera_position);
        quatf rot = mu::look_quat(dir, float3::up());
        move = to_mat3x3(rot) * move;

        m_camera_position += move;
        m_camera_target += move;
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

    const int rt_width = 1024;
    const int rt_height = 1024;
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
    m_light = m_ctx->createLight();

    m_render_target->enableReadback(true);
    m_camera->setRenderTarget(m_render_target);
    m_scene->setCamera(m_camera);
    m_scene->addLight(m_light);

    auto texture = m_ctx->createTexture(512, 512, gpt::Format::RGBAu8);
    m_mat_reflective = m_ctx->createMaterial();
    m_mat_emissive = m_ctx->createMaterial();
    m_mat_checker = m_ctx->createMaterial();


    {
        RawVector<unorm8x4> image;
        image.resize(texture->getWidth() * texture->getHeight());
        GenerateCheckerImage(image.data(), texture->getWidth(), texture->getHeight());
        texture->upload(image.cdata());
    }

    m_mat_reflective->setDiffuse(float3{ 1.0f, 1.0f, 1.0f });
    m_mat_reflective->setRoughness(0.02f);

    m_mat_emissive->setDiffuse(float3{ 0.8f, 0.8f, 0.8f });
    m_mat_emissive->setEmissive(float3{ 0.9f, 0.8f, 1.2f });

    m_mat_checker->setDiffuse(float3{ 0.9f, 0.9f, 0.9f });
    m_mat_checker->setDiffuseTexture(texture);
    //m_mat_checker->setEmissiveTexture(texture);
    {
        m_camera_position = { 0.0f, 2.0f, -8.0f };
        m_camera_target = { 0.0f, 0.0f, 0.0f };
        m_camera->setPosition(m_camera_position);
        m_camera->setDirection(mu::normalize(m_camera_target - m_camera_position));
    }
    {
        float3 pos{ 2.0f, 10.0f, -2.0f };
        float3 target{ 0.0f, 0.0f, 0.0f };
        float3 color{ 0.95f, 0.925f, 1.0f };
        m_light->setType(gpt::LightType::Point);
        m_light->setPosition(pos);
        m_light->setDirection(mu::normalize(target - pos));
        m_light->setColor(color * 1.0f);
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
        m_inst_floor = m_ctx->createMeshInstance(m_mesh_floor);
        m_inst_floor->setMaterial(m_mat_checker);
        m_scene->addMesh(m_inst_floor);
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

        m_inst_triangle = m_ctx->createMeshInstance(m_mesh_triangle);
        m_inst_triangle->setMaterial(m_mat_reflective);
        m_scene->addMesh(m_inst_triangle);
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

        m_inst_deformable = m_ctx->createMeshInstance(m_mesh_deformable);
        m_inst_deformable->setMaterial(m_mat_emissive);
        m_inst_deformable->setJointMatrices(joint_matrices);
        m_inst_deformable->setBlendshapeWeights(bs_weights);
        m_scene->addMesh(m_inst_deformable);
    }
    return true;
}

void GlimmerTest::messageLoop()
{
#ifdef EnableWindow
    {
        int frame = 0;
        while (!m_window->isClosed()) {
            m_window->processMessages();
            m_ctx->render();
            m_ctx->finish();

            float bs_weights[]{
                sin((float)frame * 0.001f) * 0.5f + 0.5f,
            };
            m_inst_deformable->setBlendshapeWeights(bs_weights);
            //m_inst_deformable->setEnabled(frame % 60 < 30);

            m_inst_triangle->setTransform(mu::to_mat4x4(mu::rotate_y(-(float)frame * 0.002f)));

            printf("%s\n", m_ctx->getTimestampLog());
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

TestCase(TestMinimum)
{
    GlimmerTest test;
    if (test.init()) {
        test.messageLoop();
    }
}

