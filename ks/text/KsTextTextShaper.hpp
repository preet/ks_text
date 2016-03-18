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

#ifndef KS_TEXT_TEXT_SHAPER_HPP
#define KS_TEXT_TEXT_SHAPER_HPP

#include <ks/KsException.hpp>
#include <ks/text/KsTextDataTypes.hpp>
#include <ks/text/KsTextGlyphDesc.hpp>

namespace ks
{
    namespace text
    {
        // References:
        // https://github.com/mapnik/mapnik/blob/master/include/mapnik/text/itemizer.hpp
        // https://github.com/arielm/Unicode/blob/master/Projects/BIDI/src/TextShaper.h

        // TextShaper
        // * Before we can shape any text, it needs to be
        //   'itemized' into contiguous groups that share:
        // - The same font
        // - The same script/lang
        // - The same direction

        // * (we don't currently do language detection)

        // =========================================================== //

        class TextShaperError : public ks::Exception
        {
        public:
            TextShaperError(std::string msg);
            ~TextShaperError() = default;
        };

        // =========================================================== //

        struct Font;

        // ShapedLine
        // * A ShapedLine represents a single line of text
        //   in @utf16text after it has been shaped
        struct ShapedLine
        {
            // start and end are indices into ShapeText's
            // @utf16text param
            uint start;
            uint end;

            // These are in visual order
            std::vector<GlyphInfo> list_glyph_info;
            std::vector<GlyphOffset> list_glyph_offsets;

            // Direction set based on the first Direction Run
            // of the entire string passed to ShapeText
            bool rtl;
        };

        // * Helper function that converts a UTF8 string to UTF16
        // * We don't use the stl because libstdc++ has a bug in
        //   codecvt_utf8_utf16 and it requires detecting endianness
        //   at compile time (?)
        // * Since we use ICU to do script detection and bidi, we
        //   may as well do the conversion with it too since this
        //   way everything will match up
        std::u16string ConvertStringUTF8ToUTF16(std::string const &utf8text);

        // ShapeText
        // * This function shapes @utf16text and returns the
        //   result in a list of ShapedLines that can be used
        //   to help render text
        // * Line breaking occurs against @max_line_width_px
        unique_ptr<std::vector<ShapedLine>>
        ShapeText(std::u16string const &utf16text,
                  std::vector<unique_ptr<Font>> const &list_fonts,
                  Hint const &text_hint);

    }
}

#endif // KS_TEXT_TEXT_SHAPER_HPP
