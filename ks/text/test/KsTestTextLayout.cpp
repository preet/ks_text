/*
   Copyright (C) 2015-2016 Preet Desai (preet.desai@gmail.com)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <ks/gl/KsGLCamera.hpp>

#include <ks/gui/KsGuiWindow.hpp>
#include <ks/gui/KsGuiApplication.hpp>
#include <ks/platform/KsPlatformMain.hpp>

#include <ks/draw/test/KsTestDrawBasicScene.hpp>
#include <ks/shared/KsImage.hpp>

#include <ks/text/KsTextTextManager.hpp>

#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

#include <ks/text/test/text_sdf_vert_glsl.hpp>
#include <ks/text/test/text_sdf_frag_glsl.hpp>

namespace test
{
    // ============================================================= //

    // VertexLayout
    using AttrType = gl::VertexBuffer::Attribute::Type;

    struct Vertex {
        glm::vec4 a_v4_position; // 16
        glm::vec2 a_v2_tex0; // 8
        glm::u8vec4 a_v4_color; // 4
    }; // sizeof == 28

    gl::VertexLayout const vx_layout {
        { "a_v4_position", AttrType::Float, 4, false },
        { "a_v2_tex0", AttrType::Float, 2, false },
        { "a_v4_color", AttrType::UByte, 4, true }
    };

    shared_ptr<draw::VertexBufferAllocator> vx_buff_allocator =
            make_shared<draw::VertexBufferAllocator>(28*6*256);

    // Buffer layout
    draw::BufferLayout const buffer_layout(
            gl::Buffer::Usage::Static,
            { vx_layout },
            { vx_buff_allocator });

    uint const g_atlas_res_px=1024;
    uint const g_glyph_res_px=24;
    uint const g_sdf_res_px=4;

    // ============================================================= //

    // Receives an 'update' signal from the scene
    class Updater : public ks::Object
    {
    public:
        using base_type = ks::Object;

        Updater(ks::Object::Key const &key,
                shared_ptr<EventLoop> evl,
                shared_ptr<Scene> scene) :
            ks::Object(key,evl),
            m_scene(scene),
            m_setup(false),
            m_draw_stage_id(0),
            m_shader_id(0),
            m_baseline_x(0),
            m_baseline_y(0)
        {
            static_assert(
                        sizeof(Vertex)==28,
                        "size of Vertex != 28");

            m_text_manager =
                    make_unique<text::TextManager>(
                        g_atlas_res_px,
                        g_glyph_res_px,
                        g_sdf_res_px);
        }

        void Init(ks::Object::Key const &,
                  shared_ptr<Updater> const &this_updater)
        {
            m_scene->signal_before_update.Connect(
                        this_updater,
                        &Updater::onUpdate,
                        ks::ConnectionType::Direct);

            m_text_manager->signal_new_atlas->Connect(
                        this_updater,
                        &Updater::onNewAtlas,
                        ConnectionType::Direct);

            m_text_manager->signal_new_glyph->Connect(
                        this_updater,
                        &Updater::onNewGlyph,
                        ConnectionType::Direct);
        }

        ~Updater() = default;


    private:
        struct TextAtlasData
        {
            Id texture_set_id;
            shared_ptr<draw::TextureSet> texture_set;

            Id uniform_set_id;
            shared_ptr<draw::UniformSet> uniform_set;

            gl::Texture2D* atlas_texture;
        };

        void onUpdate()
        {
            if(!m_setup)
            {
                // Camera
                auto window = m_scene->GetWindow();
                if(!window)
                {
                    LOG.Trace() << "TestTextLayout: Invalid window!";
                    std::abort();
                }

                auto const width_px = window->size.Get().first;
                auto const height_px = window->size.Get().second;

                m_camera.SetViewMatrixAsLookAt(
                            glm::vec3(0,0,0),
                            glm::vec3(0,0,-1),
                            glm::vec3(0,1,0));

                m_camera.SetProjMatrixAsOrtho(
                            0,               // left
                            width_px,        // right
                            height_px,       // bottom
                            0,               // top
                            -100,            // near (relative to camera eye)
                            100              // far (relative to camera eye)
                            );

                LOG.Trace() << "w " << width_px << ", h " << height_px;

                // Render setup
                auto render_system = m_scene->GetRenderSystem();
                render_system->ShowDebugText(false);

                // Add a draw stage
                m_draw_stage_id =
                        render_system->RegisterDrawStage(
                            make_shared<test::DefaultDrawStage>());

                // Add the shader
                m_shader_id =
                        render_system->RegisterShader(
                            "text_sdf",
                            text_sdf_vert_glsl,
                            text_sdf_frag_glsl);

                // Add Raster configs
                m_depth_config_id =
                        m_scene->GetRenderSystem()->RegisterDepthConfig(
                            [](ks::gl::StateSet* state_set){
                                state_set->SetDepthTest(GL_TRUE);
                                state_set->SetDepthMask(GL_FALSE);
                            });

                m_blend_config_id =
                        m_scene->GetRenderSystem()->RegisterBlendConfig(
                            [](ks::gl::StateSet* state_set){
                                state_set->SetBlend(GL_TRUE);
                                state_set->SetBlendFunction(
                                    GL_SRC_ALPHA,
                                    GL_ONE_MINUS_SRC_ALPHA,
                                    GL_SRC_ALPHA,
                                    GL_ONE_MINUS_SRC_ALPHA);
                            });

                // Add Text
                m_baseline_x = g_sdf_res_px*2;

//                std::string font_name = "LindenHill.otf";
                std::string font_name = "FiraSans-Regular.ttf";
                std::string font_path = "/home/preet/Dev/";

                m_text_manager->AddFont(
                            font_name,
                            font_path+font_name);

                m_text_hint = m_text_manager->CreateHint(font_name);


                std::string s;


                // Single line of text
                s = u8"This text shows a single line";

                auto list_lines_ptr =
                        m_text_manager->GetGlyphs(
                            text::TextManager::ConvertStringUTF8ToUTF16(s),
                            m_text_hint);

                createTextRenderData(*list_lines_ptr,glm::u8vec4{200,200,200,255});

                auto const spacing = list_lines_ptr->at(0).spacing;
                m_baseline_y += spacing;


                // Multiple lines broken with control characters
                s = u8"This text shows multiple lines\n"
                      "using control characters\n"
                      "like LF to break";

                list_lines_ptr =
                        m_text_manager->GetGlyphs(
                            text::TextManager::ConvertStringUTF8ToUTF16(s),
                            m_text_hint);

                createTextRenderData(*list_lines_ptr,glm::u8vec4{194,250,211,255});
                m_baseline_y += spacing;


                // Multiple lines broken through width limit
                s = u8"This text shows multiple lines that break "
                      "automatically when a maximum line width is specified";

                m_text_hint.max_line_width_px = width_px;

                list_lines_ptr =
                        m_text_manager->GetGlyphs(
                            text::TextManager::ConvertStringUTF8ToUTF16(s),
                            m_text_hint);

                createTextRenderData(*list_lines_ptr,glm::u8vec4{194,247,250,255});
                m_baseline_y += spacing;

                // Missing glyphs
                s = u8"This text shows missing glyphs ";
                s += 0xe2;
                s += 0x98;
                s += 0xa0; // skull
                s += 0xf0;
                s += 0x9f;
                s += 0x8d;
                s += 0x95; // pizza

                list_lines_ptr =
                        m_text_manager->GetGlyphs(
                            text::TextManager::ConvertStringUTF8ToUTF16(s),
                            m_text_hint);

                createTextRenderData(*list_lines_ptr,glm::u8vec4{250,225,194,255});
                m_baseline_y += spacing;


                // Fallback fonts
                std::string font_name_d = "NotoSansDevanagari-Regular.ttf";

                m_text_manager->AddFont(
                            font_name_d,
                            font_path+font_name_d);

                std::string fallback_list;
                fallback_list += font_name;
                fallback_list += ",";
                fallback_list += font_name_d;

                m_text_hint = m_text_manager->CreateHint(fallback_list);
                m_text_hint.script = text::Hint::Script::Multiple;
                m_text_hint.max_line_width_px = width_px;

                s = u8"This text shows font fallback (FiraSans then NotoSans-Devanagari) "
                    "\u0905\u0928\u0941\u091a\u094d\u091b\u0947\u0926 \u0031 \u2014 "
                    "\u0938\u092d\u0940 \u092e\u0928\u0941\u0937\u094d\u092f\u094b\u0902 "
                    "\u0915\u094b \u0917\u094c\u0930\u0935 \u0914\u0930 "
                    "\u0905\u0927\u093f\u0915\u093e\u0930\u094b\u0902 \u0915\u0947 "
                    "\u0935\u093f\u0937\u092f \u092e\u0947\u0902 "
                    "\u091c\u0928\u094d\u092e\u091c\u093e\u0924 "
                    "\u0938\u094d\u0935\u0924\u0928\u094d\u0924\u094d\u0930\u0924\u093e "
                    "\u0914\u0930 \u0938\u092e\u093e\u0928\u0924\u093e "
                    "\u092a\u094d\u0930\u093e\u092a\u094d\u0924 \u0939\u0948\u0902\u0964 "
                    "\u0909\u0928\u094d\u0939\u0947\u0902 \u092c\u0941\u0926\u094d\u0927\u093f "
                    "\u0914\u0930 \u0905\u0928\u094d\u0924\u0930\u093e\u0924\u094d\u092e\u093e "
                    "\u0915\u0940 \u0926\u0947\u0928 \u092a\u094d\u0930\u093e\u092a\u094d\u0924 "
                    "\u0939\u0948 \u0914\u0930 \u092a\u0930\u0938\u094d\u092a\u0930 "
                    "\u0909\u0928\u094d\u0939\u0947\u0902 \u092d\u093e\u0908\u091a\u093e\u0930\u0947 "
                    "\u0915\u0947 \u092d\u093e\u0935 \u0938\u0947 \u092c\u0930\u094d\u0924\u093e\u0935 "
                    "\u0915\u0930\u0928\u093e \u091a\u093e\u0939\u093f\u090f\u0964";

                list_lines_ptr =
                        m_text_manager->GetGlyphs(
                            text::TextManager::ConvertStringUTF8ToUTF16(s),
                            m_text_hint);

                createTextRenderData(*list_lines_ptr,glm::u8vec4{223,194,250,255});
                m_baseline_y += spacing;


                // Bidirectional text
                std::string font_name_a = "NotoNaskhArabic-Regular.ttf";

                m_text_manager->AddFont(
                            font_name_a,
                            font_path+font_name_a);

                fallback_list = font_name;
                fallback_list += ",";
                fallback_list += font_name_a;

                m_text_hint = m_text_manager->CreateHint(fallback_list);
                m_text_hint.script = text::Hint::Script::Multiple;
                m_text_hint.direction = text::Hint::Direction::Multiple;
                m_text_hint.max_line_width_px = width_px;

                s = u8"This text shows bidirectional support by mixing Arabic "
                    "(\u0627\u0644\u0639\u0631\u0628\u064a\u0629) and English. "
                    "One Thousand and One Nights "
                    "(\u0643\u0650\u062a\u064e\u0627\u0628"
                    "\u0623\u064e\u0644\u0652\u0641 \u0644\u064e\u064a\u0652\u0644\u064e\u0629 "
                    "\u0648\u064e\u0644\u064e\u064a\u0652\u0644\u064e\u0629) "
                    "and Sinbad the Sailor (\u0627\u0644\u0633\u0646\u062f\u0628\u0627\u062f "
                    "\u0627\u0644\u0628\u062d\u0631\u064a) are well known examples of Arabic literature";

                list_lines_ptr =
                        m_text_manager->GetGlyphs(
                            text::TextManager::ConvertStringUTF8ToUTF16(s),
                            m_text_hint);

                createTextRenderData(*list_lines_ptr,glm::u8vec4{248,180,180,255});
                m_baseline_y += spacing;


                // Elided text
                m_text_hint.elide = true;

                s = u8"This text shows a single line being elided when it is too long";

                list_lines_ptr =
                        m_text_manager->GetGlyphs(
                            text::TextManager::ConvertStringUTF8ToUTF16(s),
                            m_text_hint);

                createTextRenderData(*list_lines_ptr,glm::u8vec4{160,210,250,255});


                m_setup = true;
            }
        }

        void onNewAtlas(ks::uint atlas_index,ks::uint atlas_size_px)
        {
            LOG.Trace() << "onNewAtlas: " << atlas_index;

            TextAtlasData atlas_data;

            atlas_data.texture_set =
                    make_shared<draw::TextureSet>();

            atlas_data.texture_set->list_texture_desc.
                    emplace_back(
                        make_shared<gl::Texture2D>(
                            gl::Texture2D::Format::LUMINANCE8),
                        0);

            atlas_data.texture_set_id =
                    m_scene->GetRenderSystem()->
                    RegisterTextureSet(atlas_data.texture_set);


            atlas_data.uniform_set =
                    make_shared<draw::UniformSet>();

            atlas_data.uniform_set->list_uniforms.
                    push_back(
                        make_shared<gl::Uniform<GLint>>(
                            "u_s_tex0",0));

            atlas_data.uniform_set_id =
                    m_scene->GetRenderSystem()->
                    RegisterUniformSet(atlas_data.uniform_set);


            atlas_data.atlas_texture =
                    atlas_data.texture_set->
                    list_texture_desc.back().first.get();

            atlas_data.atlas_texture->SetFilterModes(
                        gl::Texture2D::Filter::Linear,
                        gl::Texture2D::Filter::Linear);

            Image<R8> blank_image(atlas_size_px,atlas_size_px,R8{0});

            atlas_data.atlas_texture->UpdateTexture(
                        gl::Texture2D::Update{
                            gl::Texture2D::Update::ReUpload,
                            glm::u16vec2(0,0),
                            shared_ptr<ImageData>(
                                blank_image.ConvertToImageDataPtr().release()
                            )
                        });

            // Save
            m_list_atlas_data.push_back(std::move(atlas_data));
        }

        void onNewGlyph(ks::uint atlas_index,
                        glm::u16vec2 offset,
                        ks::shared_ptr<ks::ImageData> image_data)
        {
            if(atlas_index > m_list_atlas_data.size()-1)
            {
                throw Exception(Exception::ErrorLevel::ERROR,
                                "Received glyph before atlas created");
            }

            auto& atlas_data = m_list_atlas_data[atlas_index];

            atlas_data.atlas_texture->UpdateTexture(
                        gl::Texture2D::Update{
                            gl::Texture2D::Update::Defaults,
                            offset,
                            image_data
                        });
        }


        void createTextRenderData(std::vector<text::Line> const &list_lines,
                                  glm::u8vec4 color)
        {
            // Build geometry
            unique_ptr<std::vector<u8>> list_vx =
                    make_unique<std::vector<u8>>();

            // Texture scaling factor
            float const k_div_atlas = 1.0/g_atlas_res_px;

            auto const m4_pv =
                    m_camera.GetProjMatrix()*
                    m_camera.GetViewMatrix();

            for(auto const &line : list_lines)
            {
                m_baseline_y += line.spacing;

                for(text::Glyph const &glyph : line.list_glyphs)
                {
                    // Original glyph dimensions (without any SDF
                    // borders)
                    uint const o_glyph_width = glyph.x1-glyph.x0;
                    uint const o_glyph_height= glyph.y1-glyph.y0;

                    if(o_glyph_width==0 || o_glyph_height==0)
                    {
                        continue;
                    }

                    uint const glyph_width = o_glyph_width + (2*glyph.sdf_x);
                    uint const glyph_height= o_glyph_height + (2*glyph.sdf_y);

                    float x0 = (glyph.x0-glyph.sdf_x);
                    float x1 = (glyph.x1+glyph.sdf_x);
                    float y0 = m_baseline_y-(glyph.y0-glyph.sdf_y);
                    float y1 = m_baseline_y-(glyph.y1+glyph.sdf_y);

                    float s0 = glyph.tex_x*k_div_atlas;
                    float s1 = (glyph.tex_x+glyph_width)*k_div_atlas;

                    // tex_y must be flipped
                    float t0 = glyph.tex_y*k_div_atlas;
                    float t1 = (glyph.tex_y+glyph_height)*k_div_atlas;

                    // BL
                    gl::Buffer::PushElement<Vertex>(
                                *list_vx,
                                Vertex{
                                    m4_pv*glm::vec4{x0,y0,0,1},
                                    glm::vec2{s0,t1},
                                    color
                                });

                    // TR
                    gl::Buffer::PushElement<Vertex>(
                                *list_vx,
                                Vertex{
                                    m4_pv*glm::vec4{x1,y1,0,1},
                                    glm::vec2{s1,t0},
                                    color
                                });

                    // TL
                    gl::Buffer::PushElement<Vertex>(
                                *list_vx,
                                Vertex{
                                    m4_pv*glm::vec4{x0,y1,0,1},
                                    glm::vec2{s0,t0},
                                    color
                                });

                    // BL
                    gl::Buffer::PushElement<Vertex>(
                                *list_vx,
                                Vertex{
                                    m4_pv*glm::vec4{x0,y0,0,1},
                                    glm::vec2{s0,t1},
                                    color
                                });

                    // BR
                    gl::Buffer::PushElement<Vertex>(
                                *list_vx,
                                Vertex{
                                    m4_pv*glm::vec4{x1,y0,0,1},
                                    glm::vec2{s1,t1},
                                    color
                                });

                    // TR
                    gl::Buffer::PushElement<Vertex>(
                                *list_vx,
                                Vertex{
                                    m4_pv*glm::vec4{x1,y1,0,1},
                                    glm::vec2{s1,t0},
                                    color
                                });
                }
            }

            // Create the entity
            auto entity_id = m_scene->CreateEntity();

            // Create the render component
            draw::DefaultDrawKey draw_key;
            draw_key.SetShader(m_shader_id);
            draw_key.SetPrimitive(gl::Primitive::Triangles);
            draw_key.SetTextureSet(m_list_atlas_data[0].texture_set_id);
            draw_key.SetUniformSet(m_list_atlas_data[0].uniform_set_id);
            draw_key.SetBlendConfig(m_blend_config_id);
            draw_key.SetDepthConfig(m_depth_config_id);

            std::vector<u8> list_draw_stages = {u8(m_draw_stage_id)};

            auto cmlist_render_data_ptr =
                    static_cast<RenderDataComponentList*>(
                        m_scene->GetComponentList<RenderData>());

            auto& render_data =
                    cmlist_render_data_ptr->Create(
                        entity_id,
                        draw_key,
                        &buffer_layout,
                        nullptr,
                        list_draw_stages,
                        ks::draw::Transparency::Opaque);

            auto& geometry = render_data.GetGeometry();
            geometry.GetVertexBuffers().push_back(std::move(list_vx));
            geometry.SetVertexBufferUpdated(0);
        }

        shared_ptr<Scene> m_scene;
        ks::gl::Camera<float> m_camera;

        unique_ptr<text::TextManager> m_text_manager;
        text::Hint m_text_hint;

        float m_baseline_x;
        float m_baseline_y;

        bool m_setup;
        Id m_draw_stage_id;
        Id m_shader_id;
        Id m_depth_config_id;
        Id m_blend_config_id;

        std::vector<TextAtlasData> m_list_atlas_data;
    };
}

// ============================================================= //
// ============================================================= //

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // Create application
    shared_ptr<gui::Application> app =
            MakeObject<gui::Application>();

    // Create window
    gui::Window::Attributes win_attribs;
    gui::Window::Properties win_props;
    win_props.swap_interval = 1;
    win_props.width = 600;
    win_props.height = 800;

    shared_ptr<gui::Window> window =
            app->CreateWindow(
                app->GetEventLoop(),
                win_attribs,
                win_props);

    shared_ptr<test::Scene> scene =
            MakeObject<test::Scene>(
                app,
                window);

    shared_ptr<test::Updater> test_updater =
            MakeObject<test::Updater>(
                app->GetEventLoop(),
                scene);

    (void)test_updater;

    // Run!
    app->Run();

    return 0;
}


// ============================================================= //
// ============================================================= //
 
