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


#include <iostream>
#include <fstream>

#include <ks/text/KsTextTextManager.hpp>
#include <ks/text/KsTextFreeType.hpp>
#include <ks/text/KsTextFont.hpp>
#include <ks/text/KsTextTextAtlas.hpp>
#include <ks/text/KsTextTextShaper.hpp>

namespace ks
{
    namespace text
    {       
        namespace {
            // FreeType context
            struct FreeTypeContext
            {
                FreeTypeContext()
                {
                    // load library
                    FT_Error error = FT_Init_FreeType(&(library));
                    if(error) {
                        std::string desc = "FreeTypeContext: "
                                           "Failed to Init FreeType: ";
                        desc += GetFreeTypeError(error);

                        throw FreeTypeError(desc);
                    }
                }

                ~FreeTypeContext()
                {
                    FT_Error error;

                    // release freetype
                    error = FT_Done_FreeType(library);
                    if(error)
                    {
                        std::string desc = "FreeTypeContext: "
                                           "Failed to close FreeType library: ";
                        desc += GetFreeTypeError(error);

                        throw FreeTypeError(desc);
                    }
                }

                // * reference to the freetype library
                FT_Library library;
            };

            shared_ptr<FreeTypeContext> g_ft_context;
        }

        FontFileInvalid::FontFileInvalid() :
            ks::Exception(ks::Exception::ErrorLevel::ERROR,"")
        {}

        // =========================================================== //

        std::string const TextManager::m_log_prefix = "TextManager: ";

        TextManager::TextManager(uint atlas_size_px,
                                 uint glyph_res_px,
                                 uint sdf_offset_px) :
            m_text_atlas(new TextAtlas(
                             atlas_size_px,
                             glyph_res_px,
                             sdf_offset_px)),
            signal_new_atlas(&(m_text_atlas->signal_new_atlas)),
            signal_new_glyph(&(m_text_atlas->signal_new_glyph))

        {
            // Create the FreeType context if it doesn't already exist
            if(g_ft_context == nullptr)
            {
                g_ft_context = make_shared<FreeTypeContext>();
            }

            // We don't init the invalid font w initial atlas
            // here because the corresponding signals can't
            // be connected to until after the constructor
        }

        TextManager::~TextManager()
        {
            cleanUpFonts();
        }

        void TextManager::AddFont(std::string font_name,
                                  std::string file_path)
        {
            if(m_list_fonts.empty())
            {
                // Create an 'invalid' font (index 0) for missing glyphs
                auto invalid_font = make_unique<Font>();
                invalid_font->name = "invalid";
                m_list_fonts.push_back(std::move(invalid_font));

                m_text_atlas->AddFont(invalid_font);
            }

            m_list_fonts.push_back(make_unique<Font>());
            auto& font = m_list_fonts.back();

            font->name = font_name;
            font->file_data = loadFontFile(file_path);

            // Load FreeType font face
            loadFreeTypeFontFace(*font);

            // Load HarfBuzz font object
            font->hb_font = hb_ft_font_create(font->ft_face,NULL);

            // Update atlas
            m_text_atlas->AddFont(font);
        }

        void TextManager::AddFont(std::string font_name,
                                  unique_ptr<std::vector<u8>> file_data)
        {
            if(m_list_fonts.empty())
            {
                // Create an 'invalid' font (index 0) for missing glyphs
                auto invalid_font = make_unique<Font>();
                invalid_font->name = "invalid";
                m_list_fonts.push_back(std::move(invalid_font));

                m_text_atlas->AddFont(invalid_font);
            }

            m_list_fonts.push_back(make_unique<Font>());
            auto& font = m_list_fonts.back();

            font->name = font_name;
            font->file_data = std::move(file_data);

            // Load FreeType font face
            loadFreeTypeFontFace(*font);

            // Load HarfBuzz font object
            font->hb_font = hb_ft_font_create(font->ft_face,NULL);

            // Update atlas
            m_text_atlas->AddFont(font);
        }

        TextHint TextManager::CreateHint(std::string const &prio_fonts,
                                         TextHint::FontSearch font_search_hint,
                                         TextHint::Direction direction_hint,
                                         TextHint::Script script_hint)
        {
            TextHint hint;

            // Create two lists of font indices, one for
            // prio_fonts and one for fallbacks

            // Split the comma-separated list of font names in
            // @prio fonts out into a list of strings
            std::istringstream ss(prio_fonts);
            std::string font_name;

            std::vector<std::string> list_prio_font_names;
            while(std::getline(ss,font_name,','))
            {
                list_prio_font_names.push_back(font_name);
            }

            uint i=0;
            for(auto const &font : m_list_fonts)
            {
                // Always skip index 0, its the 'invalid' font
                if(i==0)
                {
                    i++;
                    continue;
                }

                bool found_font=false;
                for(auto const &prio_font_name : list_prio_font_names)
                {
                    if(font->name == prio_font_name)
                    {
                        found_font = true;
                        break;
                    }
                }

                if(found_font)
                {
                    hint.list_prio_fonts.push_back(i);
                }
                else
                {
                    hint.list_fallback_fonts.push_back(i);
                }

                i++;
            }

            hint.font_search = font_search_hint;
            hint.direction = direction_hint;
            hint.script = script_hint;

            hint.glyph_res_px = m_text_atlas->GetGlyphResolutionPx();

            return hint;
        }

        void TextManager::GetGlyphs(std::string const &utf8text,
                                    TextHint const text_hint,
                                    std::vector<Glyph>& list_all_glyphs,
                                    std::vector<GlyphPosition>& list_all_glyph_pos)
        {
            // Shape with TextShaper
            std::vector<TextLine> list_lines =
                    ShapeText(utf8text,
                              m_list_fonts,
                              text_hint);

            // Create and position glyhps on each line
            uint line_spacing = 0;
            uint max_line_width = 0;
            uint total_glyph_count = 0;
            uint const line_count = list_lines.size();
            std::vector<TextLineData> list_line_data(line_count);

            for(uint i=0; i < line_count; i++)
            {
                TextLine const &line = list_lines[i];
                TextLineData &line_data = list_line_data[i];

                // Build/Rasterize the glyphs with TextAtlas
                m_text_atlas->GetGlyphs(m_list_fonts,
                                        line.list_glyph_info,
                                        line_data.list_glyphs);

                total_glyph_count += line_data.list_glyphs.size();

                // Set glyph positions on a (0,0) baseline
                // (x0,y0) for a glyph is the bottom-left
                sint x = 0;
                sint y = 0;

                uint const glyph_count = line_data.list_glyphs.size();
                line_data.list_glyph_pos.reserve(glyph_count);
                line_data.x_min = std::numeric_limits<sint>::max();
                line_data.x_max = std::numeric_limits<sint>::min();
                line_data.y_min = std::numeric_limits<sint>::max();
                line_data.y_max = std::numeric_limits<sint>::min();

                for(uint j=0; j < glyph_count; j++)
                {
                    Glyph const &glyph =
                            line_data.list_glyphs[j];

                    GlyphOffset const &glyph_offset =
                            line.list_glyph_offsets[j];

                    GlyphPosition glyph_pos;
                    glyph_pos.x0 = x + glyph_offset.offset_x + glyph.bearing_x;
                    glyph_pos.x1 = glyph_pos.x0 + glyph.width;
                    glyph_pos.y1 = y + glyph_offset.offset_y + glyph.bearing_y;
                    glyph_pos.y0 = glyph_pos.y1 - glyph.height;
                    line_data.list_glyph_pos.push_back(glyph_pos);

                    x += glyph_offset.advance_x;
                    y += glyph_offset.advance_y;

                    // update min,max x,y
                    line_data.x_min = std::min(line_data.x_min,glyph_pos.x0);
                    line_data.x_max = std::max(line_data.x_max,glyph_pos.x1);

                    line_data.y_min = std::min(line_data.y_min,glyph_pos.y0);
                    line_data.y_max = std::max(line_data.y_max,glyph_pos.y1);
                }
                // Calc line width, height and spacing
                line_data.width  = line_data.x_max-line_data.x_min;
                line_data.height = line_data.y_max-line_data.y_min;
                line_spacing = std::max(line_spacing,line_data.height);
                max_line_width = std::max(max_line_width,line_data.width);
            }

//            // buff line spacing
//            line_spacing += (line_spacing/5);

//            // clamp line spacing if requried
//            if(line_spacing > (2*m_k_glyph_world_px)) {
//                line_spacing = 2*m_k_glyph_world_px;
//            }

            // Set baseline origin x based on alignment
            // Set baseline origin y based on line_spacing
            for(uint i=0; i < list_line_data.size(); i++)
            {
                TextLineData &line_data = list_line_data[i];
                uint const glyph_count = line_data.list_glyphs.size();

                sint adj_x=0;
//                if(alignment == TEXT_ALIGN_CENTER) {
//                    adj_x = (max_line_width-line_data.width)/2;
//                }
//                else if(alignment == TEXT_ALIGN_RIGHT) {
//                    adj_x = max_line_width-line_data.width;
//                }

                for(uint j=0; j < glyph_count; j++)
                {
                    GlyphPosition &glyph_pos = line_data.list_glyph_pos[j];
                    glyph_pos.x0 += adj_x;
                    glyph_pos.x1 += adj_x;
                    glyph_pos.y0 -= (line_spacing*i);
                    glyph_pos.y1 -= (line_spacing*i);
                }
            }

            // Save
            list_all_glyphs.clear();
            list_all_glyphs.reserve(total_glyph_count);

            list_all_glyph_pos.clear();
            list_all_glyph_pos.reserve(total_glyph_count);

            for(uint i=0; i < list_line_data.size(); i++)
            {
                std::vector<Glyph> const &list_glyphs =
                        list_line_data[i].list_glyphs;

                std::vector<GlyphPosition> const &list_glyph_pos =
                        list_line_data[i].list_glyph_pos;

                list_all_glyphs.insert(list_all_glyphs.end(),
                                       list_glyphs.begin(),
                                       list_glyphs.end());

                list_all_glyph_pos.insert(list_all_glyph_pos.end(),
                                          list_glyph_pos.begin(),
                                          list_glyph_pos.end());
            }

        }

        void TextManager::cleanUpFonts()
        {
            FT_Error error;

            for(auto& font : m_list_fonts)
            {
                if(font->name == "invalid")
                {
                    continue;
                }

                // Clean up FreeType font faces
                error = FT_Done_Face(font->ft_face);
                if(error)
                {
                    std::string desc = m_log_prefix;
                    desc += "Failed to close font face: ";
                    desc += font->name;
                    desc += GetFreeTypeError(error);

                    throw FreeTypeError(desc);
                }

                // Clean up HarfBuzz font objects
                hb_font_destroy(font->hb_font);
            }
        }

        void TextManager::loadFreeTypeFontFace(Font& font)
        {
            std::vector<u8> const &file_data =
                    *(font.file_data);

            // Load font using FreeType. We only load face 0.
            FT_Error error;

            // file buff
            FT_Byte const * file_buff =
                    static_cast<FT_Byte const *>(&(file_data[0]));

            FT_Long file_size = file_data.size();

            // load face
            FT_Face &face = font.ft_face;
            error = FT_New_Memory_Face(
                        g_ft_context->library,
                        file_buff,
                        file_size,
                        0,
                        &face);

            if(error) {
                std::string desc = "Failed to load face 0 of font: ";
                desc += font.name;
                desc += GetFreeTypeError(error);

                throw FreeTypeError(desc);
            }

            // Force UCS-2 charmap for this font as
            // recommended by Harfbuzz
            bool set_charmap=false;
            for(int n = 0; n < face->num_charmaps; n++) {
                FT_UShort platform_id = face->charmaps[n]->platform_id;
                FT_UShort encoding_id = face->charmaps[n]->encoding_id;

                if (((platform_id == 0) && (encoding_id == 3)) ||
                    ((platform_id == 3) && (encoding_id == 1)))
                {
                    error = FT_Set_Charmap(face,face->charmaps[n]);
                    set_charmap = true;
                    break;
                }
            }

            if(!set_charmap || (set_charmap && error)) {
                std::string desc = "Failed to set UCS-2 charmap for ";
                desc += font.name;

                throw FreeTypeError(desc);
            }

            // Set size:
            // freetype specifies char dimensions in
            // 1/64th of a point
            // (point == 1/72 inch)

            auto const glyph_res_px =
                    m_text_atlas->GetGlyphResolutionPx();

            error = FT_Set_Char_Size(face,  // face
                                     glyph_res_px*64, // width  in 1/64th of points
                                     glyph_res_px*64, // height in 1/64th of points
                                     72,    // horizontal dpi
                                     72);   // vertical dpi

            if(error) {
                std::string desc = "Failed to set char size for font ";
                desc += font.name;
                desc += GetFreeTypeError(error);

                throw FreeTypeError(desc);
            }

            LOG.Info() << m_log_prefix << "Loaded font "
                       << font.name;
        }

        unique_ptr<std::vector<u8>> TextManager::loadFontFile(std::string file_path)
        {
            // Open font file and read it in
            std::ifstream ifs_font_file;
            ifs_font_file.open(file_path, std::ios::in | std::ios::binary);

            unique_ptr<std::vector<u8>> file_data =
                    make_unique<std::vector<u8>>();

            if(ifs_font_file.is_open())
            {
                // Get file size
                ifs_font_file.seekg(0,std::ios::end); // seek to end
                auto file_size_bytes = ifs_font_file.tellg();

                if(file_size_bytes==0)
                {
                    throw FontFileInvalid();
                }

                file_data->resize(file_size_bytes);

                ifs_font_file.seekg(0,std::ios::beg); // seek to beginning

                char* data_buff = reinterpret_cast<char*>(&((*file_data)[0]));
                ifs_font_file.read(data_buff,file_size_bytes);
            }

            return file_data;
        }

    }
}
