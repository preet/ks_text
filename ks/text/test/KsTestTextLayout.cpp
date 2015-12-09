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

#include <ks/text/test/text_sdf_vert_glsl.hpp>
#include <ks/text/test/text_sdf_frag_glsl.hpp>

namespace test
{
    // ============================================================= //

    // VertexLayout
    using AttrType = gl::VertexBuffer::Attribute::Type;

    struct Vertex {
        glm::vec3 a_v3_position; // 12
        glm::vec2 a_v2_tex0; // 8
        glm::u8vec4 a_v4_color; // 4
    }; // sizeof == 24

    gl::VertexLayout const vx_layout {
        { "a_v4_position", AttrType::Float, 3, false },
        { "a_v2_tex0", AttrType::Float, 2, false },
        { "a_v4_color", AttrType::UByte, 4, true }
    };

    shared_ptr<draw::VertexBufferAllocator> vx_buff_allocator =
            make_shared<draw::VertexBufferAllocator>(20*6*10);

    // Buffer layout
    draw::BufferLayout const buffer_layout(
            gl::Buffer::Usage::Static,
            { vx_layout },
            { vx_buff_allocator });

    uint const g_atlas_res_px=1024;
    uint const g_glyph_res_px=64;
    uint const g_sdf_res_px=8;

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
            m_entity_id(0)
        {
            static_assert(
                        sizeof(Vertex)==24,
                        "size of Vertex != 24");

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
                auto render_system = m_scene->GetRenderSystem();

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

                // Text
//                std::string font_name = "LindenHill.otf";
//                std::string font_path = "/home/preet/Dev/";

                std::string font_name = "FiraSans-Regular.ttf";
                std::string font_path = "/home/preet/Dev/";


                font_path += font_name;

                m_text_manager->AddFont(
                            font_name,
                            font_path);

                m_text_hint =
                        m_text_manager->CreateHint(
                            font_name,
                            text::TextHint::FontSearch::Fallback,
                            text::TextHint::Direction::Multiple,
                            text::TextHint::Script::Multiple);

                std::vector<text::Glyph> list_glyphs;
                std::vector<text::GlyphPosition> list_glyph_pos;

//                m_text_manager->GetGlyphs("aa\nbدليل",
//                                          m_text_hint,
//                                          list_glyphs,
//                                          list_glyph_pos);

                m_text_manager->GetGlyphs("abc\n",
                                          m_text_hint,
                                          list_glyphs,
                                          list_glyph_pos);



                createGlyphs(list_glyphs,list_glyph_pos);



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
                        make_unique<gl::Texture2D>(
                            gl::Texture2D::Format::LUMINANCE8),
                        0);

            atlas_data.texture_set_id =
                    m_scene->GetRenderSystem()->
                    RegisterTextureSet(atlas_data.texture_set);


            atlas_data.uniform_set =
                    make_shared<draw::UniformSet>();

            atlas_data.uniform_set->list_uniforms.
                    push_back(
                        make_unique<gl::Uniform<GLint>>(
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


        void createGlyphs(std::vector<text::Glyph> const &list_glyphs,
                          std::vector<text::GlyphPosition> const &list_glyph_pos)
        {
            // Remove any old entities if required
            if(m_entity_id == 0)
            {
                m_scene->RemoveEntity(m_entity_id);
            }

            m_entity_id = m_scene->CreateEntity();


            // Build geometry
            unique_ptr<std::vector<u8>> list_vx =
                    make_unique<std::vector<u8>>();

            list_vx->reserve(6*sizeof(Vertex)*list_glyphs.size());

            // Just some scaling factors
            float const k_div_glyph = 1.0/(g_glyph_res_px*2.5);
            float const k_div_atlas = 1.0/g_atlas_res_px;

            for(uint i=0; i < list_glyphs.size(); i++)
            {
                auto const &glyph = list_glyphs[i];
                auto const &glyph_pos = list_glyph_pos[i];

                // No need to render glyphs that are just
                // spacing characters
                if((glyph.width == 0) || (glyph.height == 0)) {
                    continue;
                }

                uint glyph_width = glyph.width+(2*glyph.sdf_x);
                uint glyph_height = glyph.height+(2*glyph.sdf_y);

                float x0 = (glyph_pos.x0-glyph.sdf_x)*k_div_glyph;
                float x1 = (glyph_pos.x1+glyph.sdf_x)*k_div_glyph;
                float y0 = (glyph_pos.y0-glyph.sdf_y)*k_div_glyph;
                float y1 = (glyph_pos.y1+glyph.sdf_y)*k_div_glyph;

//                float ax0 = (glyph_pos.x0-glyph.sdf_x/2);
//                float ax1 = (glyph_pos.x1+glyph.sdf_x/2);
//                float ay0 = (glyph_pos.y0-glyph.sdf_y/2);
//                float ay1 = (glyph_pos.y1+glyph.sdf_y/2);

//                LOG.Trace() << "glyph sdf: " << glyph.sdf_x << "," << glyph.sdf_y;
//                LOG.Trace() << "glyph w,h: " << glyph_pos.x1-glyph_pos.x0
//                            << ", " << glyph_pos.y1-glyph_pos.y0;
//                LOG.Trace() << "glyph ww,hh: " << glyph_width << ", " << glyph_height;
//                LOG.Trace() << "glyph aw,ah: " << ax1-ax0 << ", " << ay1-ay0;

                float s0 = glyph.tex_x*k_div_atlas;
                float s1 = (glyph.tex_x+glyph_width)*k_div_atlas;

                // tex_y must be flipped
                float t1 = glyph.tex_y*k_div_atlas;
                float t0 = (glyph.tex_y+glyph_height)*k_div_atlas;

                x0 -= 0.9;
                x1 -= 0.9;
                y0 += 0.25;
                y1 += 0.25;

                glm::u8vec4 color{255,255,255,255};

                // BL
                gl::Buffer::PushElement<Vertex>(
                            *list_vx,
                            Vertex{
                                glm::vec3{x0,y1,0},
                                glm::vec2{s0,t1},
                                color
                            });

                // TR
                gl::Buffer::PushElement<Vertex>(
                            *list_vx,
                            Vertex{
                                glm::vec3{x1,y0,0},
                                glm::vec2{s1,t0},
                                color
                            });

                // TL
                gl::Buffer::PushElement<Vertex>(
                            *list_vx,
                            Vertex{
                                glm::vec3{x0,y0,0},
                                glm::vec2{s0,t0},
                                color
                            });

                // BL
                gl::Buffer::PushElement<Vertex>(
                            *list_vx,
                            Vertex{
                                glm::vec3{x0,y1,0},
                                glm::vec2{s0,t1},
                                color
                            });

                // BR
                gl::Buffer::PushElement<Vertex>(
                            *list_vx,
                            Vertex{
                                glm::vec3{x1,y1,0},
                                glm::vec2{s1,t1},
                                color
                            });

                // TR
                gl::Buffer::PushElement<Vertex>(
                            *list_vx,
                            Vertex{
                                glm::vec3{x1,y0,0},
                                glm::vec2{s1,t0},
                                color
                            });
            }

            // Create the render component

            // FIXME
            auto depth_config_id =
                    m_scene->GetRenderSystem()->RegisterDepthConfig(
                        [](ks::gl::StateSet* state_set){
                            state_set->SetDepthTest(GL_TRUE);
                            state_set->SetDepthMask(GL_FALSE);
                        });

            auto blend_config_id =
                    m_scene->GetRenderSystem()->RegisterBlendConfig(
                        [](ks::gl::StateSet* state_set){
                            state_set->SetBlend(GL_TRUE);
                            state_set->SetBlendFunction(
                                GL_SRC_ALPHA,
                                GL_ONE_MINUS_SRC_ALPHA,
                                GL_SRC_ALPHA,
                                GL_ONE_MINUS_SRC_ALPHA);
                        });


            draw::DefaultDrawKey draw_key;
            draw_key.SetShader(m_shader_id);
            draw_key.SetPrimitive(gl::Primitive::Triangles);
            draw_key.SetTextureSet(m_list_atlas_data[0].texture_set_id);
            draw_key.SetUniformSet(m_list_atlas_data[0].uniform_set_id);
            draw_key.SetBlendConfig(blend_config_id);
            draw_key.SetDepthConfig(depth_config_id);

            std::vector<u8> list_draw_stages = {u8(m_draw_stage_id)};

            auto cmlist_render_data_ptr =
                    static_cast<RenderDataComponentList*>(
                        m_scene->GetComponentList<RenderData>());

            auto& render_data =
                    cmlist_render_data_ptr->Create(
                        m_entity_id,
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

        unique_ptr<text::TextManager> m_text_manager;
        text::TextHint m_text_hint;

        bool m_setup;
        Id m_draw_stage_id;
        Id m_shader_id;
        Id m_entity_id;

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
    win_props.width = 600;
    win_props.height = 600;

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

    (void)test_updater;

    // Run!
    app->Run();

    // Stop threads
    EventLoop::RemoveFromThread(scene_evl,scene_thread,true);
    EventLoop::RemoveFromThread(render_evl,render_thread,true);

    return 0;
}

// ============================================================= //
// ============================================================= //
 
