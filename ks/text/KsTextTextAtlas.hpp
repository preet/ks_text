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

#ifndef KS_TEXT_TEXT_ATLAS_HPP
#define KS_TEXT_TEXT_ATLAS_HPP

#include <glm/gtc/type_precision.hpp>

#include <ks/KsSignal.hpp>
#include <ks/shared/KsImage.hpp>
#include <ks/shared/KsBinPackShelf.hpp>
#include <ks/text/KsTextGlyphDesc.hpp>

namespace ks
{
    struct ImageData;

    namespace text
    {
        struct Font;

        // =========================================================== //

        class TextAtlasError : public ks::Exception
        {
        public:
            TextAtlasError(std::string msg);
            ~TextAtlasError() = default;
        };

        // =========================================================== //

        class TextAtlas final
        {
            friend class TextManager;

        private:
            TextAtlas(uint atlas_size_px=1024,
                      uint glyph_res_px=32,
                      uint sdf_offset_px=4);

            void AddFont(unique_ptr<Font> const &font);

            void GetGlyphs(std::vector<unique_ptr<Font>> const &list_fonts,
                           std::vector<GlyphInfo> const &list_glyph_info,
                           std::vector<GlyphImageDesc> &list_glyphs);

            uint GetAtlasSizePx() const;
            uint GetGlyphResolutionPx() const;
            uint GetSDFOffsetPx() const;

        public:
            ~TextAtlas();

            // atlas index,
            // atlas size px,
            Signal<
                uint,
                uint
            > signal_new_atlas;

            // atlas index,
            // image offset,
            // image data
            Signal<
                uint,
                glm::u16vec2,
                shared_ptr<ImageData>
            > signal_new_glyph;

        private:
            void genGlyph(std::vector<unique_ptr<Font>> const &list_fonts,
                          GlyphInfo const &glyph_info,
                          GlyphImageDesc &glyph);

            std::vector<GlyphImageDesc>::iterator findGlyph(uint font_index,
                                                   uint glyph_index);


            void assignMissingGlyph(unique_ptr<Font> const &font);
            void genMissingGlyph();
            void addEmptyAtlas();

            // lower bound compare predicate
            static bool glyphIsLessThanLB(GlyphImageDesc const &glyph,u32 index);

            // upper bound compare predicate
            static bool glyphIsLessThanUB(u32 index,GlyphImageDesc const &glyph);


            static const std::string m_log_prefix;

            uint const m_atlas_size_px;
            uint const m_glyph_res_px;
            uint const m_sdf_offset_px;

            // missing_glyph
            // * universal 'missing' glyph used when
            //   a character isn't available for a font
            GlyphImageDesc m_missing_glyph;

            // lkup_font_glyph_list
            // * Glyph lists indexed by font
            // * Each glyph list is ordered by glyph index
            using GlyphList = std::vector<GlyphImageDesc>;
            std::vector<std::vector<GlyphImageDesc>> m_lkup_font_glyph_list;

            // list_atlas_bins
            // * list of packing bins and images for all glyphs
            // * atlases aren't sorted by font or any other
            //   criteria and are created as they fill up
            std::vector<BinPackShelf> m_list_atlas_bins;
        };
    }
}

#endif // KS_TEXT_TEXT_ATLAS_HPP
