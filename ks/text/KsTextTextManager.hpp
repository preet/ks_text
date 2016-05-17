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

#ifndef KS_TEXT_TEXT_MANAGER_HPP
#define KS_TEXT_TEXT_MANAGER_HPP

#include <glm/gtc/type_precision.hpp>

#include <ks/KsSignal.hpp>
#include <ks/KsException.hpp>
#include <ks/text/KsTextDataTypes.hpp>
#include <ks/text/KsTextGlyphDesc.hpp>

namespace ks
{
    struct ImageData;

    namespace text
    {
        // TextShaper:
        // * inputs: utf8text,font hints,max_width,direction hint
        // * outputs: glyph info, glyph offsets

        // TextAtlas:
        // * inputs: glyph info
        // * outputs: glyph images and metrics

        // TextManager:
        // * combine glyph offsets and glyph metrics and images
        //   to provide the final glyphs and their positions

        // =========================================================== //

        class FontFileInvalid : public ks::Exception
        {
        public:
            FontFileInvalid();
            ~FontFileInvalid() = default;
        };

        class NoFontsAvailable : public ks::Exception
        {
        public:
            NoFontsAvailable();
            ~NoFontsAvailable() = default;
        };

        class HintInvalid : public ks::Exception
        {
        public:
            HintInvalid(std::string);
            ~HintInvalid() = default;
        };

        // =========================================================== //

        class TextAtlas;
        struct Font;

        class TextManager
        {
            static const std::string m_log_prefix;

            // text_atlas is up here to avoid warnings about
            // the init order wrt signals
            unique_ptr<TextAtlas> m_text_atlas;

        public:
            enum class Alignment
            {
                Left,
                Center,
                Right
            };

            TextManager(uint atlas_size_px=1024,
                        uint glyph_res_px=32,
                        uint sdf_offset_px=4);

            ~TextManager();

            void AddFont(std::string font_name,
                         std::string file_path);

            void AddFont(std::string font_name,
                         unique_ptr<std::vector<u8>> file_data);

            Hint CreateHint(std::string const &list_prio_fonts="");

            unique_ptr<std::vector<Line>>
            GetGlyphs(std::u16string const &utf16text,
                      Hint const &text_hint);

            static std::u16string
            ConvertStringUTF8ToUTF16(std::string const &utf8text);

            static std::string
            ConvertStringUTF32ToUTF8(std::u32string const &utf32text);

            // uint: atlas index
            Signal<uint,uint> * const signal_new_atlas;

            // uint: atlas index,
            // glm::u16vec2: image offset,
            // shared_ptr<ImageData>: image data
            Signal<
                uint,
                glm::u16vec2,
                shared_ptr<ImageData>
            > * const signal_new_glyph;


        private:
            void initFreeType();
            void loadFreeTypeFontFace(Font& font);
            void cleanUpFreeType();
            void cleanUpFonts();

            unique_ptr<std::vector<u8>> loadFontFile(std::string file_path);

            std::vector<unique_ptr<Font>> m_list_fonts;
        };
    }
}

#endif // KS_TEXT_TEXT_MANAGER_HPP
