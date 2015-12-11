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

#include <freetypegl/make_distance_map.hpp>

#include <ks/KsLog.hpp>
#include <ks/text/KsTextTextAtlas.hpp>
#include <ks/text/KsTextFreeType.hpp>
#include <ks/text/KsTextFont.hpp>

namespace ks
{
    namespace text
    {
        TextAtlasError::TextAtlasError(std::string msg) :
            ks::Exception(ks::Exception::ErrorLevel::ERROR,std::move(msg))
        {}

        // =========================================================== //

        const std::string TextAtlas::m_log_prefix = "TextAtlas: ";

        TextAtlas::TextAtlas(uint atlas_size_px,
                             uint glyph_res_px,
                             uint sdf_offset_px) :
            m_atlas_size_px(atlas_size_px),
            m_glyph_res_px(glyph_res_px),
            m_sdf_offset_px(sdf_offset_px)
        {

        }

        TextAtlas::~TextAtlas()
        {

        }

        void TextAtlas::AddFont(unique_ptr<Font> const &font)
        {
            m_lkup_font_glyph_list.push_back(GlyphList());

            if(m_lkup_font_glyph_list.size() == 1)
            {
                // Setup the initial 'invalid' font
                addEmptyAtlas();
                genMissingGlyph();
            }
            else
            {
                // Assign a custom missing glyph
                // to this font if required
                assignMissingGlyph(font);
            }
        }

        void TextAtlas::GetGlyphs(std::vector<unique_ptr<Font>> const &list_fonts,
                                  std::vector<GlyphInfo> const &list_glyph_info,
                                  std::vector<GlyphImageDesc> &list_glyphs)
        {
            for(auto const &glyph_info : list_glyph_info)
            {
                // Check for the zero-dimension glyphs first
                if(glyph_info.zero_width)
                {
                    GlyphImageDesc empty_glyph;
                    empty_glyph.font  = glyph_info.font;
                    empty_glyph.index = glyph_info.index;
                    empty_glyph.atlas = 0;
                    // (texture)
                    empty_glyph.tex_x = 0;
                    empty_glyph.tex_y = 0;
                    // (sdf)
                    empty_glyph.sdf_x = 0;
                    empty_glyph.sdf_y = 0;
                    // (metrics)
                    empty_glyph.bearing_x = 0;
                    empty_glyph.bearing_y = 0;
                    empty_glyph.width     = 0;
                    empty_glyph.height    = 0;

                    list_glyphs.push_back(empty_glyph);
                }
                else
                {
                    auto glyph_it = findGlyph(glyph_info.font,glyph_info.index);

                    if(glyph_it == m_lkup_font_glyph_list[glyph_info.font].end())
                    {
                        GlyphImageDesc new_glyph;
                        genGlyph(list_fonts,glyph_info,new_glyph);

                        list_glyphs.push_back(new_glyph);
                    }
                    else
                    {
                        list_glyphs.push_back(*glyph_it);
                    }
                }
            }
        }

        uint TextAtlas::GetAtlasSizePx() const
        {
            return m_atlas_size_px;
        }

        uint TextAtlas::GetGlyphResolutionPx() const
        {
            return m_glyph_res_px;
        }

        uint TextAtlas::GetSDFOffsetPx() const
        {
            return m_sdf_offset_px;
        }

        void TextAtlas::genGlyph(std::vector<unique_ptr<Font>> const &list_fonts,
                                 GlyphInfo const &glyph_info,
                                 GlyphImageDesc &glyph)
        {
            if(glyph_info.font == 0) {
                std::string desc = m_log_prefix;
                desc += "Glyph gen: invalid font";

                throw TextAtlasError(desc);
            }

            // Render glyph to the active glyph slot
            FT_Face &face = list_fonts[glyph_info.font]->ft_face;

            FT_Error const error =
                    FT_Load_Glyph(face,glyph_info.index,FT_LOAD_RENDER);

            if(error) {
                std::string desc = m_log_prefix;
                desc += "Failed to render glyph: Font: ";
                desc += list_fonts[glyph_info.font]->name;
                desc += ", index: ";
                desc += glyph_info.index;
                desc += ": ";
                desc += GetFreeTypeError(error);

                throw FreeTypeError(desc);
            }

            // Get glyph metrics
            // NOTE: Most glyph metrics params are expressed in 26.6
            //       fractional pixel format:
            //       '64' in 26.6 format == 1 pixel
            //        So to get pixels, divide by 64
            FT_Glyph_Metrics &metrics = face->glyph->metrics;
            u32 metrics_width_px  = metrics.width/64;
            u32 metrics_height_px = metrics.height/64;

            // If this glyph is just a 'spacing' character,
            // save it without generating a texture and return
            if((metrics_width_px == 0) || (metrics_height_px == 0)) {
                // Save glyph
                // (ref)
                glyph.font  = glyph_info.font;
                glyph.index = glyph_info.index;
                glyph.atlas = 0;
                // (texture)
                glyph.tex_x = 0;
                glyph.tex_y = 0;
                // (sdf)
                glyph.sdf_x = m_sdf_offset_px;
                glyph.sdf_y = m_sdf_offset_px;
                // (metrics)
                glyph.bearing_x = metrics.horiBearingX/64;
                glyph.bearing_y = metrics.horiBearingY/64;
                glyph.width     = metrics_width_px;
                glyph.height    = metrics_height_px;

                // TODO not sure if this should be saved here
                auto& list_glyphs = m_lkup_font_glyph_list[glyph_info.font];

                std::vector<GlyphImageDesc>::iterator glyph_it;
                glyph_it = std::upper_bound(list_glyphs.begin(),
                                            list_glyphs.end(),
                                            glyph.index,
                                            glyphIsLessThanUB);

                list_glyphs.insert(glyph_it,glyph);

                return;
            }

            BinPackRectangle glyph_rect;
            glyph_rect.width  = (metrics_width_px) + 2*m_sdf_offset_px;
            glyph_rect.height = (metrics_height_px) + 2*m_sdf_offset_px;

            // Try to add the glyph rect into an atlas;
            // create a new atlas if current ones are full
            BinPackShelf * atlas_bin = &(m_list_atlas_bins.back());
            if(!(atlas_bin->AddRectangle(glyph_rect))) {
                this->addEmptyAtlas();
                atlas_bin = &(m_list_atlas_bins.back());
                atlas_bin->AddRectangle(glyph_rect);

                // TODO if the second add fails, we should
                // throw; the glyph size might be bigger than
                // the atlas size
            }

            // Add the glyph bitmap with a position offset
            unique_ptr<std::vector<R8>> glyph_subimage_data =
                    make_unique<std::vector<R8>>();

            // Copy the glyph bitmap from freetype
            // We expect a single byte/pixel
            FT_Bitmap &bitmap = face->glyph->bitmap;
            int const abs_pitch = abs(bitmap.pitch);
            assert(bitmap.width == abs_pitch);

            // Reserve space to avoid reallocation
            glyph_subimage_data->reserve(
                        bitmap.rows*bitmap.width);

            // A negative pitch indicates the first byte of
            // the image represents the bottom left corner
            int offset = (bitmap.pitch > 0) ?
                        0 : (abs_pitch * (bitmap.rows-1));

            // copy byte by byte
            for(int r=0; r < bitmap.rows; r++) {
                for(int c=0; c < bitmap.width; c++) {
                    // save the gray levels in the red channel
                    glyph_subimage_data->push_back(
                                R8{bitmap.buffer[offset+c]});
                }
                offset += bitmap.pitch;
            }

            // Create the glyph subimage
            Image<R8> glyph_subimage;
            glyph_subimage.SetAll(metrics_width_px,
                                  metrics_height_px,
                                  std::move(glyph_subimage_data));

            // Create the glyph image (ie subimage + space
            // for the SDF transform)
            Image<R8> glyph_image(glyph_rect.width,
                                  glyph_rect.height,
                                  R8{0});

            auto sdf_ins_pixel_it =
                    glyph_image.GetPixel(
                        m_sdf_offset_px,
                        m_sdf_offset_px);

            glyph_image.Insert(glyph_subimage,
                               glyph_subimage.GetPixel(0,0),
                               sdf_ins_pixel_it);


            // Apply sdf transform
            u8* glyph_image_bytes =
                    reinterpret_cast<u8*>(
                        &(glyph_image.GetData()[0]));

            make_distance_map(glyph_image_bytes,
                              glyph_image.GetWidth(),
                              glyph_image.GetHeight());

            // Save glyph
            // (ref)
            glyph.font  = glyph_info.font;
            glyph.index = glyph_info.index;
            glyph.atlas = m_list_atlas_bins.size()-1;
            // (texture)
            glyph.tex_x = glyph_rect.x;
            glyph.tex_y = glyph_rect.y;
            // (sdf)
            glyph.sdf_x = m_sdf_offset_px;
            glyph.sdf_y = m_sdf_offset_px;
            // (metrics)
            glyph.bearing_x = metrics.horiBearingX/64.0;
            glyph.bearing_y = metrics.horiBearingY/64.0;
            glyph.width     = metrics_width_px;
            glyph.height    = metrics_height_px;

            auto& list_glyphs = m_lkup_font_glyph_list[glyph_info.font];

            std::vector<GlyphImageDesc>::iterator glyph_it;
            glyph_it = std::upper_bound(list_glyphs.begin(),
                                        list_glyphs.end(),
                                        glyph.index,
                                        glyphIsLessThanUB);

            list_glyphs.insert(glyph_it,glyph);

            // Notify listeners
            signal_new_glyph.Emit(
                        glyph.atlas,
                        glm::u16vec2(
                            glyph_rect.x,
                            glyph_rect.y),
                        shared_ptr<ks::ImageData>(
                            glyph_image.
                            ConvertToImageDataPtr().release()));
        }

        std::vector<GlyphImageDesc>::iterator TextAtlas::findGlyph(uint font_index,
                                                          uint glyph_index)
        {
            auto& list_glyphs =
                    m_lkup_font_glyph_list[font_index];

            auto glyph_it =
                    std::lower_bound(
                        list_glyphs.begin(),
                        list_glyphs.end(),
                        glyph_index,
                        glyphIsLessThanLB);

            if(glyph_it != list_glyphs.end())
            {
                // test to see if glyph_it is pointing
                // to a glyph with an equivalent index
                // or just an adjacent element
                if(glyph_it->index != glyph_index)
                {
                    return list_glyphs.end();
                }
            }

            return glyph_it;
        }

        // rn: assignMissingGlyphIfReq
        void TextAtlas::assignMissingGlyph(unique_ptr<Font> const &font)
        {
            // Generate a missing glyph for this font if
            // it doesn't have one.

            // We assume that @font belongs to the last added font

            FT_Face &face = font->ft_face;
            FT_Error const error = FT_Load_Glyph(face,0,FT_LOAD_RENDER);

            if(error)
            {
                std::string desc = m_log_prefix;
                desc += "Failed to render missing glyph: Font: ";
                desc += font->name;
                desc += ": ";
                desc += GetFreeTypeError(error);

                throw FreeTypeError(desc);
            }

            // The missing glyph must both have non-zero
            // dimensions and a bitmap that isn't blank to
            // be considered valid
            FT_Glyph_Metrics &metrics = face->glyph->metrics;
            u32 const metrics_width_px  = metrics.width/64;
            u32 const metrics_height_px = metrics.height/64;

            if(metrics_width_px*metrics_height_px == 0)
            {
                auto& list_glyphs = m_lkup_font_glyph_list.back();
                list_glyphs.insert(list_glyphs.begin(),m_missing_glyph);
                list_glyphs.back().font = m_lkup_font_glyph_list.size()-1;

                return;
            }

            // Check that the bitmap has non-zero pixels
            FT_Bitmap &bitmap = face->glyph->bitmap;
            int const abs_pitch = abs(bitmap.pitch);

            int offset = (bitmap.pitch > 0) ?
                        0 : (abs_pitch * (bitmap.rows-1));

            bool pixel_filled = false;
            for(int r=0; r < bitmap.rows; r++)
            {
                for(int c=0; c < bitmap.width; c++)
                {
                    if(bitmap.buffer[offset+c] > 0)
                    {
                        pixel_filled = true;
                        break;
                    }
                }
                offset += bitmap.pitch;
            }

            if(pixel_filled == false)
            {
                auto& list_glyphs = m_lkup_font_glyph_list.back();
                list_glyphs.insert(list_glyphs.begin(),m_missing_glyph);
                list_glyphs.back().font = m_lkup_font_glyph_list.size()-1;
                return;
            }
        }

        void TextAtlas::genMissingGlyph()
        {
            // create the missing glyph bitmap

            float dim = m_glyph_res_px;
            float adj = m_sdf_offset_px;
            float th = dim/5.0;

            float x0 = floor(th*1)+adj;
            float x1 = floor(th*1.75)+adj;
            float x2 = floor(th*3.25)+adj;
            float x3 = floor(th*4)+adj;

            float y0 = floor(th*0.5)+adj;
            float y1 = floor(th*1.25)+adj;
            float y2 = floor(th*3.75)+adj;
            float y3 = floor(th*4.5)+adj;

            float dim_full = dim+(2*adj);

            ks::Image<ks::R8> glyph_image(dim_full,dim_full,ks::R8{0});
            auto& glyph_image_data = glyph_image.GetData();

            uint const dim_full_i = dim_full;
            for(uint i=0; i < glyph_image_data.size(); i++) {
                u16 x = i%dim_full_i;
                u16 y = i/dim_full_i;

                bool in1 = ((x > x0) && (x < x3)) && ((y > y0) && (y < y3));
                bool in2 = ((x >= x1) && (x <= x2)) && ((y >= y1) && (y <= y2));

                if(in1 && (!in2)) {
                    glyph_image_data[i].r = 255;
                }
            }

            // apply sdf transform
            u8* glyph_image_bytes =
                    reinterpret_cast<u8*>(
                        &(glyph_image_data[0]));

            make_distance_map(glyph_image_bytes,dim_full,dim_full);

            // add to atlas
            BinPackRectangle glyph_rect;
            glyph_rect.width  = dim_full;
            glyph_rect.height = dim_full;

            // Try to add the glyph rect into an atlas;
            // create a new atlas if current ones are full
            BinPackShelf * atlas_bin = &(m_list_atlas_bins.back());
            atlas_bin->AddRectangle(glyph_rect);

            // notify that a glyph was created
            signal_new_glyph.Emit(
                        0,
                        glm::u16vec2(
                            glyph_rect.x,
                            glyph_rect.y),
                        shared_ptr<ks::ImageData>(
                            glyph_image.
                            ConvertToImageDataPtr().release()));

            // save glyph
            // (ref)
            m_missing_glyph.font  = 0;
            m_missing_glyph.index = 0;
            m_missing_glyph.atlas = 0;
            // (texture)
            m_missing_glyph.tex_x = glyph_rect.x;
            m_missing_glyph.tex_y = glyph_rect.y;
            // (sdf)
            m_missing_glyph.sdf_x = m_sdf_offset_px;
            m_missing_glyph.sdf_y = m_sdf_offset_px;
            // (metrics)
            m_missing_glyph.bearing_x = 0;
            m_missing_glyph.bearing_y = dim;
            m_missing_glyph.width     = dim;
            m_missing_glyph.height    = dim;
        }

        void TextAtlas::addEmptyAtlas()
        {
            BinPackShelf atlas_bin(m_atlas_size_px,m_atlas_size_px,1);
            m_list_atlas_bins.push_back(atlas_bin);

            signal_new_atlas.Emit(
                        m_list_atlas_bins.size()-1,
                        m_atlas_size_px);
        }

        bool TextAtlas::glyphIsLessThanLB(GlyphImageDesc const &glyph,
                                          u32 index)
        {
            return (glyph.index < index);
        }

        bool TextAtlas::glyphIsLessThanUB(u32 index,
                                          GlyphImageDesc const &glyph)
        {
            return (index < glyph.index);
        }

    }
}
