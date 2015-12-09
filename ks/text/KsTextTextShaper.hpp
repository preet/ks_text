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

        // TextLine
        // * A TextLine is a run of text meant to represent
        //   a single line created as the result of line
        //   breaking a TextParagraph
        // * The start and end indices are codepoint offsets
        //   in TextParagraph.utf16text
        // * A given TextLine may span multiple TextRuns;
        //   'start' and 'end' are determined via line breaking
        struct TextLine
        {
            u32 start;
            u32 end;

            // combined advance
            //s32 width;

            // these are in VISUAL order
            std::vector<GlyphInfo> list_glyph_info;
            std::vector<GlyphOffset> list_glyph_offsets;
        };

        std::vector<TextLine>
        ShapeText(std::string const &utf8text,
                  std::vector<unique_ptr<Font>> const &list_fonts,
                  TextHint const &text_hint,
                  uint const max_line_width_px=std::numeric_limits<uint>::max());

    }
}

#endif // KS_TEXT_TEXT_SHAPER_HPP
