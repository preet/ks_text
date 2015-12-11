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
#include <locale>
#include <codecvt>

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

        // =========================================================== //

        FontFileInvalid::FontFileInvalid() :
            ks::Exception(ks::Exception::ErrorLevel::ERROR,"")
        {}

        NoFontsAvailable::NoFontsAvailable() :
            ks::Exception(ks::Exception::ErrorLevel::ERROR,"")
        {}

        HintInvalid::HintInvalid() :
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

        Hint TextManager::CreateHint(std::string const &prio_fonts,
                                     Hint::FontSearch font_search_hint,
                                     Hint::Direction direction_hint,
                                     Hint::Script script_hint)
        {
            if(m_list_fonts.empty())
            {
                throw NoFontsAvailable();
            }

            Hint hint;

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

        namespace {

            // Helper
            template<typename T>
            void OrderedUniqueInsert(std::vector<T>& list_data,T ins_data)
            {
                // lower bound == first value thats greater than or equal
                auto it = std::lower_bound(list_data.begin(),
                                           list_data.end(),
                                           ins_data);

                // insert when: it==end, *it != ins_data (ins_data is greater)
                if((it==list_data.end()) || (*it != ins_data) ) {
                    list_data.insert(it,ins_data);
                }
            }

        }

        unique_ptr<std::vector<Line>>
        TextManager::GetGlyphs(std::u16string const &utf16text,
                               Hint const &text_hint,
                               uint const max_line_width_px)
        {
            if(text_hint.list_prio_fonts.empty() &&
               text_hint.list_fallback_fonts.empty())
            {
                throw HintInvalid();
            }

            auto list_lines_ptr = make_unique<std::vector<Line>>();
            if(utf16text.empty())
            {
                return list_lines_ptr;
            }

            uint const invalid_font_line_height =
                    m_text_atlas->GetGlyphResolutionPx() +
                    (m_text_atlas->GetGlyphResolutionPx()/5);

            // Shape with TextShaper
            auto list_shaped_lines_ptr =
                    ShapeText(utf16text,
                              m_list_fonts,
                              text_hint,
                              max_line_width_px);

            auto& list_shaped_lines = *list_shaped_lines_ptr;

            // Create and position glyhps on each line
            list_lines_ptr->resize(list_shaped_lines.size());
            auto& list_lines = *list_lines_ptr;

            // For each line
            for(uint i=0; i < list_shaped_lines.size(); i++)
            {
                ShapedLine const &shaped_line = list_shaped_lines[i];
                Line &line = list_lines[i];

                // TODO add start and end of each line
                line.start = shaped_line.start;
                line.end = shaped_line.end;

                uint const glyph_count =
                        shaped_line.list_glyph_info.size();

                std::vector<GlyphImageDesc> list_glyph_imgs;


                // Build/Rasterize the glyphs with TextAtlas
                m_text_atlas->GetGlyphs(
                            m_list_fonts,
                            shaped_line.list_glyph_info,
                            list_glyph_imgs);

                // Set glyph positions on a (0,0) baseline.
                // (x0,y0) for a glyph is the bottom-left
                sint pen_x = 0;

                line.x_min = std::numeric_limits<sint>::max();
                line.x_max = std::numeric_limits<sint>::min();
                line.y_min = std::numeric_limits<sint>::max();
                line.y_max = std::numeric_limits<sint>::min();

                line.list_glyphs.resize(glyph_count);

                std::vector<uint> list_unq_fonts;

                // For each glyph
                for(uint j=0; j < glyph_count; j++)
                {
                    GlyphImageDesc const &glyph_img =
                            list_glyph_imgs[j];

                    GlyphOffset const &glyph_offset =
                            shaped_line.list_glyph_offsets[j];

                    Glyph& glyph = line.list_glyphs[j];

                    glyph.cluster = shaped_line.list_glyph_info[j].cluster;

                    glyph.atlas = glyph_img.atlas;
                    glyph.tex_x = glyph_img.tex_x;
                    glyph.tex_y = glyph_img.tex_y;
                    glyph.sdf_x = glyph_img.sdf_x;
                    glyph.sdf_y = glyph_img.sdf_y;

                    glyph.x0 = pen_x + glyph_offset.offset_x + glyph_img.bearing_x;
                    glyph.x1 = glyph.x0 + glyph_img.width;
                    glyph.y1 = glyph_offset.offset_y + glyph_img.bearing_y;
                    glyph.y0 = glyph.y1 - glyph_img.height;

                    pen_x += glyph_offset.advance_x;

                    // update min,max x,y
                    line.x_min = std::min(line.x_min,glyph.x0);
                    line.x_max = std::max(line.x_max,glyph.x1);
                    line.y_min = std::min(line.y_min,glyph.y0);
                    line.y_max = std::max(line.y_max,glyph.y1);

                    // update unique font list
                    OrderedUniqueInsert<uint>(list_unq_fonts,glyph_img.font);
                }

                // Calculate the line spacing
                line.spacing = 0;
                for(auto font : list_unq_fonts)
                {
                    // Fix the invalid font height
                    if(font == 0)
                    {
                        line.spacing = std::max(line.spacing,invalid_font_line_height);
                    }
                    else
                    {
                        auto const &ft_size_metrics =
                                m_list_fonts[font]->ft_face->size->metrics;

                        uint font_line_height = ft_size_metrics.height/64;
                        line.spacing = std::max(line.spacing,font_line_height);
                    }
                }
            }

            return list_lines_ptr;
        }

        std::u16string TextManager::ConvertStringUTF8ToUTF16(std::string const &utf8text)
        {
            return text::ConvertStringUTF8ToUTF16(utf8text);
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
