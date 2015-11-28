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
#include <ks/text/KsTextDataTypes.hpp>

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

            void AddFont();

            void GetGlyphs(std::vector<unique_ptr<Font>> const &list_fonts,
                           std::vector<GlyphInfo> const &list_glyph_info,
                           std::vector<Glyph> &list_glyphs);

            uint GetAtlasSizePx() const;
            uint GetGlyphResolutionPx() const;
            uint GetSDFOffsetPx() const;

        public:
            ~TextAtlas();

            Signal<uint> signal_new_atlas;

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
                          Glyph &glyph);

            std::vector<Glyph>::iterator findGlyph(uint font_index,
                                                   uint glyph_index);

            void genMissingGlyph();

            void addEmptyAtlas();

            // lower bound compare predicate
            static bool glyphIsLessThanLB(Glyph const &glyph,u32 index);

            // upper bound compare predicate
            static bool glyphIsLessThanUB(u32 index,Glyph const &glyph);


            static const std::string m_log_prefix;

            uint const m_atlas_size_px;
            uint const m_glyph_res_px;
            uint const m_sdf_offset_px;

            // missing_glyph
            // * universal 'missing' glyph used when
            //   a character isn't available for a font
            Glyph m_missing_glyph;

            // lkup_font_glyph_list
            // * Glyph lists indexed by font
            using GlyphList = std::vector<Glyph>;
            std::vector<std::vector<Glyph>> m_lkup_font_glyph_list;

            // list_atlas_bins
            // * list of packing bins and images for all glyphs
            // * atlases aren't sorted by font or any other
            //   criteria and are created as they fill up
            std::vector<BinPackShelf> m_list_atlas_bins;
        };
    }
}

#endif // KS_TEXT_TEXT_ATLAS_HPP
