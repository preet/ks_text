/*
   Copyright (C) 2015 Preet Desai (preet.desai@gmail.com)

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

#include <ks/gui/KsGuiWindow.hpp>
#include <ks/gui/KsGuiApplication.hpp>
#include <ks/platform/KsPlatformMain.hpp>

#include <ks/draw/test/KsTestDrawBasicScene.hpp>
#include <ks/shared/KsImage.hpp>

#include <ks/text/KsTextTextManager.hpp>

#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace test
{
    // ============================================================= //

    // Shader
    std::string const vertex_shader =
                "#ifdef GL_ES\n"
                "    //\n"
                "#else\n"
                "    #define lowp\n"
                "    #define mediump\n"
                "    #define highp\n"
                "#endif\n"
                "\n"
                "attribute vec4 a_v4_position;\n"
                "attribute vec2 a_v2_tex0;\n"
                "\n"
                "varying lowp vec2 v_v2_tex0;\n"
                "\n"
                "void main()\n"
                "{\n"
                "   v_v2_tex0 = a_v2_tex0;\n"
                "   gl_Position = a_v4_position;\n"
                "}\n";

    std::string const frag_shader =
                "#ifdef GL_ES\n"
                "    precision mediump float;\n"
                "#else\n"
                "    #define lowp\n"
                "    #define mediump\n"
                "    #define highp\n"
                "#endif\n"
                "\n"
                "varying lowp vec2 v_v2_tex0;\n"
                "uniform lowp sampler2D u_s_tex0;\n"
                "\n"
                "void main()\n"
                "{\n"
                "    gl_FragColor = texture2D(u_s_tex0,v_v2_tex0);\n"
                "}\n";


    // ============================================================= //

    // VertexLayout
    using AttrType = gl::VertexBuffer::Attribute::Type;

    struct Vertex {
        glm::vec3 a_v3_position; // 12
        glm::vec2 a_v2_tex0; // 8
    }; // sizeof == 20

    gl::VertexLayout const vx_layout {
        { "a_v4_position", AttrType::Float, 3, false },
        { "a_v2_tex0", AttrType::Float, 2, false }
    };

    shared_ptr<draw::VertexBufferAllocator> vx_buff_allocator =
            make_shared<draw::VertexBufferAllocator>(20*6*10);

    // Buffer layout
    draw::BufferLayout const buffer_layout(
            gl::Buffer::Usage::Static,
            { vx_layout },
            { vx_buff_allocator });

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
            m_shader_id(0)
        {
            m_text_manager = make_unique<text::TextManager>(128);
        }

        void Init(ks::Object::Key const &,
                  shared_ptr<Updater> const &this_updater)
        {
            m_scene->signal_before_update.Connect(
                        this_updater,
                        &Updater::onUpdate);

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


        void OnTextInput(std::string utf8str)
        {
            std::vector<text::Glyph> list_glyphs;
            std::vector<text::GlyphPosition> list_glyph_pos;

            m_text_manager->GetGlyphs(utf8str,
                                      m_text_hint,
                                      list_glyphs,
                                      list_glyph_pos);
        }


    private:
        struct TextAtlasData
        {
            Id texture_set_id;
            shared_ptr<draw::TextureSet> texture_set;

            Id uniform_set_id;
            shared_ptr<draw::UniformSet> uniform_set;

            gl::Texture2D* atlas_texture;

            Id entity_id;
        };

        void initAtlases()
        {
            auto render_system = m_scene->GetRenderSystem();

            for(uint i=0; i < 4; i++)
            {
                TextAtlasData atlas_data;

                // Create an empty texture
                Image<R8> empty_image(16,16,R8{100});

                auto empty_texture =
                        make_unique<gl::Texture2D>(
                            gl::Texture2D::Format::LUMINANCE8);

                empty_texture->UpdateTexture(
                            gl::Texture2D::Update{
                                gl::Texture2D::Update::ReUpload,
                                glm::u16vec2(0,0),
                                shared_ptr<ImageData>(
                                    empty_image.ConvertToImageDataPtr().release())
                            });

                empty_texture->SetFilterModes(
                            gl::Texture2D::Filter::Linear,
                            gl::Texture2D::Filter::Linear);

                // (texture set)
                atlas_data.texture_set = make_shared<draw::TextureSet>();
                atlas_data.texture_set->list_texture_desc.emplace_back(
                            std::move(empty_texture),0);

                atlas_data.texture_set_id =
                        render_system->RegisterTextureSet(
                            atlas_data.texture_set);

                atlas_data.atlas_texture =
                        atlas_data.texture_set->list_texture_desc.back().first.get();

                LOG.Trace() << "new texture set id : " << atlas_data.texture_set_id;


                // (uniform set)
                atlas_data.uniform_set = make_shared<draw::UniformSet>();
                atlas_data.uniform_set->list_uniforms.push_back(
                            make_unique<gl::Uniform<GLint>>("u_s_tex0",0));

                atlas_data.uniform_set_id =
                        render_system->RegisterUniformSet(
                            atlas_data.uniform_set);


                // Create the entity and render data
                atlas_data.entity_id = m_scene->CreateEntity();

                float x0 = (i%2 == 0) ? -1.0 : 0.0;
                float x1 = x0+1.0;
                float y0 = (i < 2) ? 1.0 : 0.0;
                float y1 = y0-1.0;

                unique_ptr<std::vector<u8>> list_vx =
                        make_unique<std::vector<u8>>();

                // BL
                gl::Buffer::PushElement<Vertex>(
                            *list_vx,
                            Vertex{
                                glm::vec3{x0,y1,0},
                                glm::vec2{0,1}
                            });

                // TR
                gl::Buffer::PushElement<Vertex>(
                            *list_vx,
                            Vertex{
                                glm::vec3{x1,y0,0},
                                glm::vec2{1,0}
                            });

                // TL
                gl::Buffer::PushElement<Vertex>(
                            *list_vx,
                            Vertex{
                                glm::vec3{x0,y0,0},
                                glm::vec2{0,0}
                            });

                // BL
                gl::Buffer::PushElement<Vertex>(
                            *list_vx,
                            Vertex{
                                glm::vec3{x0,y1,0},
                                glm::vec2{0,1}
                            });

                // BR
                gl::Buffer::PushElement<Vertex>(
                            *list_vx,
                            Vertex{
                                glm::vec3{x1,y1,0},
                                glm::vec2{1,1}
                            });

                // TR
                gl::Buffer::PushElement<Vertex>(
                            *list_vx,
                            Vertex{
                                glm::vec3{x1,y0,0},
                                glm::vec2{1,0}
                            });

                // Create the render component
                draw::DefaultDrawKey draw_key;
                draw_key.SetShader(m_shader_id);
                draw_key.SetPrimitive(gl::Primitive::Triangles);
                draw_key.SetTextureSet(atlas_data.texture_set_id);
                draw_key.SetUniformSet(atlas_data.uniform_set_id);

                std::vector<u8> list_draw_stages = {u8(m_draw_stage_id)};

                auto cmlist_render_data_ptr =
                        static_cast<RenderDataComponentList*>(
                            m_scene->GetComponentList<RenderData>());

                auto& render_data =
                        cmlist_render_data_ptr->Create(
                            atlas_data.entity_id,
                            draw_key,
                            &buffer_layout,
                            nullptr,
                            list_draw_stages,
                            ks::draw::Transparency::Opaque);

                auto& geometry = render_data.GetGeometry();
                geometry.GetVertexBuffers().push_back(std::move(list_vx));
                geometry.SetVertexBufferUpdated(0);

                m_list_atlas_data.push_back(atlas_data);
            }
        }

        void onUpdate()
        {
            if(!m_setup)
            {
                auto render_system = m_scene->GetRenderSystem();

                // Add a draw stage
                m_draw_stage_id =
                        render_system->RegisterDrawStage(
                            make_shared<test::DefaultDrawStage>());

                // Add the shader
                m_shader_id =
                        render_system->RegisterShader(
                            "flat_tex",
                            test::vertex_shader,
                            test::frag_shader);

                // Create Atlases
                initAtlases();

                // Text
                m_text_manager->AddFont(
                            "FiraSans-Regular.ttf",
                            "/home/preet/Dev/FiraSans-Regular.ttf");

                m_text_hint =
                        m_text_manager->CreateHint(
                            "FiraSans-Regular.ttf",
                            text::TextHint::FontSearch::Fallback,
                            text::TextHint::Direction::Multiple,
                            text::TextHint::Script::Multiple);

                std::vector<text::Glyph> list_glyphs;
                std::vector<text::GlyphPosition> list_glyph_pos;

                m_setup = true;
            }
        }

        void onNewAtlas(ks::uint atlas_index)
        {
            if(atlas_index > 3) {
                return;
            }

            LOG.Trace() << "adding atlas " << atlas_index;

            auto& atlas_data = m_list_atlas_data[atlas_index];

            Image<R8> blank_image(128,128,R8{0});

            atlas_data.atlas_texture->UpdateTexture(
                        gl::Texture2D::Update{
                            gl::Texture2D::Update::ReUpload,
                            glm::u16vec2(0,0),
                            shared_ptr<ImageData>(
                                blank_image.ConvertToImageDataPtr().release()
                            )
                        });
        }

        void onNewGlyph(ks::uint atlas_index,
                        glm::u16vec2 offset,
                        ks::shared_ptr<ks::ImageData> image_data)
        {
            if(atlas_index > 3) {
                return;
            }

            LOG.Trace() << "adding glyph for " << atlas_index
                        << ", " << offset.x << "," << offset.y
                        << " | " << image_data->width << ", " << image_data->height;

            auto& atlas_data = m_list_atlas_data[atlas_index];

            atlas_data.atlas_texture->UpdateTexture(
                        gl::Texture2D::Update{
                            gl::Texture2D::Update::Defaults,
                            offset,
                            image_data
                        });
        }



        shared_ptr<Scene> m_scene;

        unique_ptr<text::TextManager> m_text_manager;
        text::TextHint m_text_hint;

        bool m_setup;
        Id m_draw_stage_id;
        Id m_shader_id;
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
            make_object<gui::Application>();

    // Create the render thread
    shared_ptr<EventLoop> render_evl = make_shared<EventLoop>();

    std::thread render_thread =
            EventLoop::LaunchInThread(render_evl);

    // Create the scene thread
    shared_ptr<EventLoop> scene_evl = make_shared<EventLoop>();

    std::thread scene_thread =
            EventLoop::LaunchInThread(scene_evl);


    // Create window
    gui::Window::Attributes win_attribs;
    gui::Window::Properties win_props;
    win_props.width = 480;
    win_props.height = 480;

    shared_ptr<gui::Window> window =
            app->CreateWindow(
                render_evl,
                win_attribs,
                win_props);

    shared_ptr<test::Scene> scene =
            make_object<test::Scene>(
                scene_evl,
                window,
                std::chrono::milliseconds(15));

    shared_ptr<test::Updater> test_updater =
            make_object<test::Updater>(
                scene_evl,
                scene);

    app->signal_utf8_input->Connect(
                test_updater,
                &test::Updater::OnTextInput);

    // Run!
    app->Run();

    // Stop threads
    EventLoop::RemoveFromThread(scene_evl,scene_thread,true);
    EventLoop::RemoveFromThread(render_evl,render_thread,true);

    return 0;
}

// ============================================================= //
// ============================================================= //
