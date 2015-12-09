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

#include <catch/catch.hpp>

#include <ks/text/KsTextTextManager.hpp>

namespace ks_test_text
{
    std::map<uint,uint> g_lkup_atlas_glyph_count;

    void OnNewAtlas(ks::uint atlas_index)
    {
        g_lkup_atlas_glyph_count[atlas_index] = 0;
    }

    void OnNewGlyph(ks::uint atlas_index,
                    glm::u16vec2,
                    ks::shared_ptr<ks::ImageData>)
    {
        g_lkup_atlas_glyph_count[atlas_index] += 1;
    }
}

//void it()
TEST_CASE("TextShaper","[textshaper]")
{
    using namespace ks_test_text;

    ks::text::TextManager text_manager;

    text_manager.signal_new_atlas->Connect(
                &OnNewAtlas);

    text_manager.signal_new_glyph->Connect(
                &OnNewGlyph);

    text_manager.AddFont("FiraSans-Regular.ttf",
                         "/home/preet/Dev/FiraSans-Regular.ttf");

    REQUIRE(g_lkup_atlas_glyph_count.size()==1);
    REQUIRE(g_lkup_atlas_glyph_count[0]==1);

    auto text_hint =
            text_manager.CreateHint(
                "FiraSans-Regular.ttf",
                ks::TextHint::FontSearch::Fallback,
                ks::TextHint::Direction::Multiple,
                ks::TextHint::Script::Multiple);

    std::vector<ks::Glyph> list_glyphs;
    std::vector<ks::GlyphPosition> list_glyph_pos;

    text_manager.GetGlyphs("hello",
                           text_hint,
                           list_glyphs,
                           list_glyph_pos);
}
