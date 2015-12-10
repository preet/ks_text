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

#ifndef KS_TEXT_DATATYPES_HPP
#define KS_TEXT_DATATYPES_HPP

#include <vector>
#include <ks/KsGlobal.hpp>

namespace ks
{
    namespace text
    {
        // =========================================================== //

        struct Hint
        {
            enum class FontSearch
            {
                Fallback,
                Explicit
            };

            enum class Script
            {
                Single,
                Multiple
            };

            enum class Direction
            {
                LeftToRight,
                RightToLeft,
                Multiple
            };

            std::vector<uint> list_prio_fonts;
            std::vector<uint> list_fallback_fonts;

            FontSearch font_search{FontSearch::Fallback};
            Direction direction{Direction::LeftToRight};
            Script script{Script::Single};

            uint glyph_res_px;
        };

        // =========================================================== //

        struct Glyph
        {
            uint cluster;
            uint atlas;

            // texture coords (pixels) for the top
            // left corner of the glyph tex in its atlas
            u16 tex_x;
            u16 tex_y;

            // sdf quad <--> glyph offset vector (pixels)
            u16 sdf_x;
            u16 sdf_y;

            s32 x0;
            s32 y0;
            s32 x1; // x1 is to the right of x0
            s32 y1; // y1 is below y0
        };

        // =========================================================== //

        struct Line
        {
            uint start;
            uint end;

            sint x_min;
            sint x_max;
            sint y_min; // equivalent to descent
            sint y_max; // equivalent to ascent

            // This is the vertical spacing between this line
            // and the next, determined by getting the max line
            // height of all the font faces used in this line.
            // Its set by the font designer and not necessarily
            // equal to y_max-y_min
            uint spacing;

            std::vector<Glyph> list_glyphs;
        };

        // =========================================================== //
    }
} // raintk

#endif // KS_TEXT_DATATYPES_HPP
