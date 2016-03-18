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

            // The width at which line breaking (or eliding) occurs
            uint max_line_width_px{std::numeric_limits<uint>::max()};

            // Sets whether or not text will be elided. If true,
            // text will be truncated before the line width limit
            // is reached and appended with '...' at the end
            bool elide{false};
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
            s32 y1; // y1 is above y0
        };

        // =========================================================== //

        struct Line
        {
            uint start;
            uint end;

            // Bounding box
            // * Bounding box for all glyphs in this line
            sint x_min;
            sint x_max;
            sint y_min;
            sint y_max;

            // Font Metrics
            // * Font metric values are the absolute maximum
            //   value for each font used in this line

            // * Distance above the baseline to enclose all glyphs
            sint ascent;

            // * Distance below the baseline to enclose all glyphs
            //   (generally negative)
            sint descent;

            // * Vertical spacing between baselines
            uint spacing;

            // * The list of indices for each atlas
            std::vector<uint> list_atlases;

            std::vector<Glyph> list_glyphs;

            // * Overall direction for the paragraph this
            //   line belongs to (rtl stands for right-to-left)
            bool rtl;
        };

        // =========================================================== //
    }
} // raintk

#endif // KS_TEXT_DATATYPES_HPP
