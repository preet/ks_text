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

#ifndef KS_TEXT_FONT_HPP
#define KS_TEXT_FONT_HPP

#include <ks/text/KsTextFreeType.hpp>

#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ft.h>

namespace ks
{
	namespace text
	{
		struct Font
        {
            std::string name;

            unique_ptr<std::vector<u8>> file_data;

            // FreeType reference for this font
            // (we only use face 0 of the font)
            FT_Face ft_face;

            // HarfBuzz reference for this font
            hb_font_t* hb_font;
        };
	}
}

#endif // KS_TEXT_FONT_HPP 
