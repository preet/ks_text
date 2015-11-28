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

#include <sstream>
#include <algorithm>

#include <icu/common/unicode/unistr.h>
#include <icu/common/unicode/ubidi.h>
#include <icu/common/unicode/uscript.h>
#include <icu/common/unicode/schriter.h>
#include <icu/extra/scrptrun.h>

#include <unibreak/linebreak.h>

#include <ks/KsLog.hpp>
#include <ks/text/KsTextTextShaper.hpp>
#include <ks/text/KsTextFreeType.hpp>
#include <ks/text/KsTextFont.hpp>

namespace ks
{
    namespace text
    {
        // =========================================================== //    

        TextShaperError::TextShaperError(std::string msg) :
            ks::Exception(ks::Exception::ErrorLevel::ERROR,std::move(msg))
        {}

        // =========================================================== //

        namespace {
            //

            // Run
            // * A run is a contiguous grouping of glyphs
            // * UTF16 is used for the indices because thats
            //   what ICU uses
            struct Run
            {
                Run(u32 start, u32 end) :
                    start(start),
                    end(end)
                {
                    // empty
                }

                u32 start;
                u32 end;
            };

            // FontRun, ScriptLangRun, DirectionRun
            // * run with the same font, script, direction
            struct FontRun : Run
            {
                FontRun(u32 start,
                        u32 end,
                        uint font) :
                    Run(start,end),
                    font(font)
                {
                    // empty
                }

                uint font;
            };

            struct ScriptLangRun : Run

            {
                ScriptLangRun(u32 start,
                              u32 end,
                              hb_script_t script) :
                    Run(start,end),
                    script(script)
                {
                    // empty
                }

                hb_script_t script;
            };

            struct DirectionRun : Run
            {
                DirectionRun(u32 start,
                             u32 end,
                             hb_direction_t dirn) :
                    Run(start,end),
                    dirn(dirn)
                {
                    // empty
                }

                hb_direction_t dirn;
            };

            // TextRun
            // * A text run is a run where each glyph in
            //   between start and end share the same font,
            //   script and direction
            // * The start and end indices are codepoint offsets
            //   in TextParagraph.utf16text
            struct TextRun : Run
            {
                TextRun() :
                    Run(0,0)
                {
                    // empty
                }

                uint font;
                hb_script_t script;
                hb_direction_t dirn;
            };


            // TextParagraph
            // * A TextParagraph is the container object that holds
            //   data required for shaping a single text string
            struct TextParagraph
            {
                icu::UnicodeString utf16text;
                u32 utf16count; // num utf16 indices
                u32 codepoint_count;

                std::vector<FontRun> list_font_runs;
                std::vector<ScriptLangRun> list_script_runs;
                std::vector<DirectionRun> list_dirn_runs;
                std::vector<TextRun> list_runs;

                std::vector<TextLine> list_lines;

                // clusters where we can break
                std::vector<u32> list_breaks;
            };

            // =========================================================== //

            void ItemizeFont(std::vector<unique_ptr<Font>> const &list_fonts,
                             TextHint const &text_hint,
                             TextParagraph &para)
            {
                // StringCharacterIterator iterates through the
                // string by codepoint not code units, so num.
                // of iterations might != utf16text.length

                // Specifically a single code point may be made
                // up of multiple code units (ie in utf8 and utf16)
                icu::StringCharacterIterator utf16char_it(para.utf16text);

                // Get the font index for each glyph
                std::vector<uint> list_glyph_fonts;

                // utf16text.length() == num code units, not num code points
                list_glyph_fonts.reserve(para.utf16text.length());

                if(text_hint.font_search == TextHint::FontSearch::Explicit)
                {
                    // If the FontSearch mode is Explicit, we only search
                    // the specified font and set a missing glyph if no
                    // corresponding character exists
                    auto const &font = list_fonts[text_hint.list_prio_fonts[0]];

                    while(utf16char_it.hasNext())
                    {
                        u32 const unicode = utf16char_it.next32PostInc();

                        // We determine whether or not the codepoint is
                        // available for this font using FT_Get_Char_Index;
                        // this is probably faster than checking glyphs
                        // saved under font->list_glyphs.
                        // TODO: look into FTC_CMapCache_Lookup

                        FT_UInt const glyph_index =
                                FT_Get_Char_Index(font->ft_face,unicode);

                        if(glyph_index == 0) {
                            list_glyph_fonts.push_back(0);
                        }
                        else {
                            list_glyph_fonts.push_back(text_hint.list_prio_fonts[0]);
                        }
                    }
                }
                else
                {
                    // We create a copy of the fallback fonts because we might
                    // rearrange them as we search
                    auto list_fallback_fonts = text_hint.list_fallback_fonts;

                    // If the FontSearch mode is Fallback, we search through
                    // all fonts to find a match for each glyph
                    while(utf16char_it.hasNext())
                    {
                        FT_UInt glyph_index = 0;
                        u32 const unicode = utf16char_it.next32PostInc();

                        // Check the priority fonts first
                        for(auto const idx : text_hint.list_prio_fonts)
                        {
                            auto const &font = list_fonts[idx];

                            glyph_index = FT_Get_Char_Index(font->ft_face,unicode);
                            if(glyph_index != 0)
                            {
                                list_glyph_fonts.push_back(idx);
                                break;
                            }
                        }

                        if(glyph_index != 0)
                        {
                            continue;
                        }

                        // Check the fallback fonts
                        for(auto const idx : list_fallback_fonts)
                        {
                            auto const &font = list_fonts[idx];

                            glyph_index = FT_Get_Char_Index(font->ft_face,unicode);
                            if(glyph_index != 0)
                            {
                                // Move the current font index to the front of the list
                                if(idx != 0)
                                {
                                    auto idx_it = std::next(list_fallback_fonts.begin(),idx);
                                    list_fallback_fonts.erase(idx_it);
                                    list_fallback_fonts.insert(list_fallback_fonts.begin(),idx);
                                }

                                list_glyph_fonts.push_back(idx);
                                break;
                            }
                        }

                        // There's no valid font for this glyph
                        if(glyph_index == 0)
                        {
                            list_glyph_fonts.push_back(0);
                        }
                    }
                }

                // Group the text into runs with the same font
                para.list_font_runs.reserve(para.utf16text.length()+1);
                para.list_font_runs.push_back(
                            FontRun(0,1,std::numeric_limits<uint>::max())); // dummy to make adding runs easier

                u32 utf16char_pos=0;
                std::vector<uint>::const_iterator font_it;
                for(font_it  = list_glyph_fonts.begin();
                    font_it != list_glyph_fonts.end(); ++font_it)
                {
                    if(para.list_font_runs.back().font != (*font_it)) {
                        para.list_font_runs.back().end = utf16char_pos;
                        para.list_font_runs.push_back(
                                    FontRun(utf16char_pos,
                                            utf16char_pos+1,
                                            (*font_it)));
                    }
                    utf16char_pos++;
                }
                // remove dummy and set last font run end
                para.list_font_runs.erase(para.list_font_runs.begin());
                para.list_font_runs.back().end = list_glyph_fonts.size();
            }

            // =========================================================== //

            hb_script_t IcuScriptToHB(UScriptCode script)
            {
                if (script == USCRIPT_INVALID_CODE) {
                    return HB_SCRIPT_INVALID;
                }

                return hb_script_from_string(uscript_getShortName(script), -1);
            }


            hb_direction_t IcuDirectionToHB(UBiDiDirection direction)
            {
                return (direction == UBIDI_RTL) ? HB_DIRECTION_RTL : HB_DIRECTION_LTR;
            }


            void ItemizeScript(TextParagraph &para)
            {
                para.list_script_runs.reserve(para.utf16text.length());

                icu_extra::ScriptRun script_run(para.utf16text.getBuffer(),
                                                para.utf16text.length());
                while(script_run.next()) {
                    ScriptLangRun run(script_run.getScriptStart(),
                                      script_run.getScriptEnd(),
                                      IcuScriptToHB(script_run.getScriptCode()));

                    para.list_script_runs.push_back(run);
                }
            }

            // =========================================================== //

            void ItemizeDirection(TextParagraph &para,
                                  hb_direction_t const dirn_hint)
            {
                // 0 sets overall to LTR
                UBiDiLevel overall_dirn = 0;

                if(dirn_hint == HB_DIRECTION_INVALID) {
                    // UBIDI_DEFAULT_LTR will set the overall direction
                    // based on the first strong directional character in
                    // the text. If no strong char is available, ICU will
                    // default to LTR.
                    overall_dirn = UBIDI_DEFAULT_LTR;
                }
                else {
                    overall_dirn = (dirn_hint == HB_DIRECTION_LTR) ? 0 : 1;
                }

                s32 const length = para.utf16text.length();
                UErrorCode error = U_ZERO_ERROR;
                UBiDi * bidi = ubidi_openSized(length,  // max text length
                                               0,       // max num. of runs (0 == auto)
                                               &error);

                if(bidi == NULL || U_FAILURE(error)) {
                    std::string desc = "TextShaper: ItemizeDirection: ";
                    if(bidi==NULL) {
                        desc += "bidi NULL";
                    }
                    else {
                        desc += u_errorName(error);
                    }

                    throw TextShaperError(desc);
                }

                // divide the text into direction runs using
                // the unicode bidi algorithm
                ubidi_setPara(bidi,
                              para.utf16text.getBuffer(),
                              length,
                              overall_dirn,
                              NULL,
                              &error);

                if(U_FAILURE(error)) {
                    std::string desc = "TextShaper: ItemizeDirection: ";
                    desc += u_errorName(error);

                    ubidi_close(bidi);

                    throw TextShaperError(desc);
                }

                // save runs
                UBiDiDirection direction = ubidi_getDirection(bidi);
                if(direction != UBIDI_MIXED) {
                    // single unidirectional run
                    para.list_dirn_runs.push_back(
                        DirectionRun(0,length,
                                     IcuDirectionToHB(direction)));
                }
                else {
                    // multiple bidi runs
                    s32 count = ubidi_countRuns(bidi,&error);
                    if(U_FAILURE(error)) {
                        std::string desc = "TextShaper: ItemizeDirection: ";
                        desc += "Failed to count BiDi runs: ";
                        desc += u_errorName(error);

                        ubidi_close(bidi);

                        throw TextShaperError(desc);
                    }

                    for(s32 i=0; i < count; i++) {
                        s32 start, length;
                        direction = ubidi_getVisualRun(bidi,i,&start,&length);
                        para.list_dirn_runs.push_back(
                            DirectionRun(start,start+length,
                                         IcuDirectionToHB(direction)));
                    }
                }

                ubidi_close(bidi);
            }

            // =========================================================== //

            void MergeRuns(TextParagraph &para)
            {
                // This method creates a TextRun for every run that shares
                // the same font, script and direction so that the runs
                // can be shaped by Harfbuzz

                // Text runs need to be in visual order

                // Direction runs are output in visual order so we
                // use those as the base thats divided up to create
                // Text runs with the same Font, Script and Direction

                // NOTE: When RTL runs are divided into Text runs, we
                // need to reverse the order of all Text runs within
                // that direction run

                // Example
                // (assume no spaces)
                // Codepoints:  0--3  3--6  6-8   8--11 11--14
                // Logical:     ARA1  HEB2  Eng   ARA3   HEB4

                // Script Runs: (0-3):ARA,
                //              (3-6):HEB,
                //              (6-8):LAT,
                //              (8-11):ARA,
                //              (11-14):HEB

                // Font Runs:   (0-3):Arabic font,
                //              (3-6):Hebrew font,
                //              (6-8):Latin font,
                //              (8-11):Arabic font,
                //              (11-14):Hebrew font

                // Dirn Runs:   1. (8-14): RTL
                //              2. (6-8): LTR
                //              3. (0-6): RTL

                // Text Runs:   1. (11-14): HEB, Hebrew font, RTL // *
                //              2. (8-11): ARA, Arabic font, RTL // *
                //              3. (6-8):  Eng, English font, LTR
                //              4. (3-6): HEB, Hebrew font, RTL // *
                //              5. (0-3): ARA, Arabic font, RTL // *
                //
                // * note the reverse order of text runs within
                //   the same RTL run

                para.list_runs.reserve(para.codepoint_count); // worst case

                std::vector<std::vector<Run const *>> list_all_runs(2); // font, script

                // Add all font runs
                for(size_t i=0; i < para.list_font_runs.size(); i++) {
                    list_all_runs[0].push_back(&(para.list_font_runs[i]));
                }

                // Add all script runs
                for(size_t i=0; i < para.list_script_runs.size(); i++) {
                    list_all_runs[1].push_back(&(para.list_script_runs[i]));
                }

                // Use directions as the base and divide into segments
                // based on all other runs
                std::vector<DirectionRun>::const_iterator dirn_it;
                for(dirn_it  = para.list_dirn_runs.begin();
                    dirn_it != para.list_dirn_runs.end(); ++dirn_it)
                {
                    u32 text_run_start = dirn_it->start;

                    std::vector<TextRun>::iterator rtl_insert_it =
                            para.list_runs.end();

                    while(text_run_start < dirn_it->end) {

                        u32 text_run_end = dirn_it->end;

                        // Get the indices for the font and script runs
                        // that contain text_run_start
                        std::vector<u32> list_run_idxs(2);

                        for(size_t i=0; i < list_all_runs.size(); i++)
                        {
                            for(size_t j=0; j < list_all_runs[i].size(); j++)
                            {
                                Run const * run = list_all_runs[i][j];
                                if((run->start <= text_run_start) &&
                                   (run->end > text_run_start))
                                {
                                    list_run_idxs[i] = j;
                                    text_run_end = std::min(text_run_end,run->end);
                                }
                            }
                        }

                        // create text run
                        TextRun text_run;
                        text_run.start = text_run_start;
                        text_run.end = text_run_end;
                        text_run.font = para.list_font_runs[list_run_idxs[0]].font;
                        text_run.script = para.list_script_runs[list_run_idxs[1]].script;
                        text_run.dirn = dirn_it->dirn;

                        if(text_run.dirn == HB_DIRECTION_LTR) {
                            para.list_runs.push_back(text_run);
                        }
                        else {
                            // adjacent RTL runs are placed before the prev RTL run
                            rtl_insert_it = para.list_runs.insert(rtl_insert_it,text_run);
                        }

                        text_run_start = text_run_end;
                    }
                }
            }

            // =========================================================== //

            void ShapeLine(std::vector<unique_ptr<Font>> const &list_fonts,
                           TextHint const &text_hint,
                           TextParagraph &para,
                           u32 const line_idx)
            {
                hb_buffer_t * hb_buff = hb_buffer_create();
                TextLine &line = para.list_lines[line_idx];
                line.list_glyph_info.clear();
                line.list_glyph_offsets.clear();

                // for each text run
                std::vector<TextRun>::const_iterator run_it;
                for(run_it  = para.list_runs.begin();
                    run_it != para.list_runs.end(); ++run_it)
                {
                    // If line and this run don't overlap, skip
                    if((line.start > (run_it->end-1)) ||
                       ((line.end-1) < run_it->start))
                    {
                        continue;
                    }

                    u32 start_idx  = std::max(line.start,run_it->start);
                    u32 end_idx    = std::min(line.end,run_it->end);

                    // If this run has no valid font available, replace
                    // all of its chars with a missing glyph
                    if(run_it->font == 0)
                    {
                        size_t const run_length = end_idx - start_idx;

                        s32 start,next;
                        if(run_it->dirn == HB_DIRECTION_LTR) {
                            start   = start_idx;
                            next    = 1;
                        }
                        else {
                            start   = end_idx-1;
                            next    = -1;
                        }
                        for(size_t i=0; i < run_length; i++) {
                            // Use the 'Missing' glyph
                            u32 cluster = start + (i*next);

                            GlyphInfo glyph_info;
                            glyph_info.font = 0;
                            glyph_info.index = 0;
                            glyph_info.cluster = cluster;

                            GlyphOffset glyph_offset;
                            glyph_offset.advance_x = text_hint.glyph_res_px;
                            glyph_offset.advance_y = 0;
                            glyph_offset.offset_x  = 0;
                            glyph_offset.offset_y  = 0;

                            line.list_glyph_info.push_back(glyph_info);
                            line.list_glyph_offsets.push_back(glyph_offset);
                        }
                        continue;
                    }

                    // prepare harfbuzz
                    hb_buffer_clear_contents(hb_buff);
                    hb_buffer_set_script(hb_buff,run_it->script);
                    hb_buffer_set_direction(hb_buff,run_it->dirn);

                    hb_buffer_add_utf16(hb_buff,
                                        para.utf16text.getBuffer(),
                                        para.utf16text.length(),
                                        start_idx,
                                        end_idx - start_idx);

                    // shape!
                    hb_shape(list_fonts[run_it->font]->hb_font,
                             hb_buff,NULL,0);

                    u32 const glyph_count =
                            hb_buffer_get_length(hb_buff);

                    hb_glyph_info_t * hb_list_glyph_info =
                            hb_buffer_get_glyph_infos(hb_buff,NULL);

                    hb_glyph_position_t * hb_list_glyph_pos =
                            hb_buffer_get_glyph_positions(hb_buff,NULL);

                    line.list_glyph_info.reserve(glyph_count);
                    line.list_glyph_offsets.reserve(glyph_count);

                    // save glyph info and offsets
                    for(size_t i=0; i < glyph_count; i++)
                    {
                        hb_glyph_info_t const &hb_glyph_info =
                                hb_list_glyph_info[i];

                        hb_glyph_position_t const &hb_glyph_pos =
                                hb_list_glyph_pos[i];

                        if(hb_glyph_info.codepoint) {
                            GlyphInfo glyph_info;
                            glyph_info.index     = hb_glyph_info.codepoint;
                            glyph_info.cluster   = hb_glyph_info.cluster;
                            glyph_info.font      = run_it->font;
                            line.list_glyph_info.push_back(glyph_info);

                            GlyphOffset glyph_offset;
                            glyph_offset.advance_x = hb_glyph_pos.x_advance/64.0;
                            glyph_offset.advance_y = hb_glyph_pos.y_advance/64.0;
                            glyph_offset.offset_x  = hb_glyph_pos.x_offset/64.0;
                            glyph_offset.offset_y  = hb_glyph_pos.y_offset/64.0;
                            line.list_glyph_offsets.push_back(glyph_offset);

                            // debug
                            // SYNCSGLOG << "--" << i << "--";
                            // SYNCSGLOG << "adv_x: " << int(glyph_offset.advance_x);
                            // SYNCSGLOG << "adv_y: " << int(glyph_offset.advance_y);
                            // SYNCSGLOG << "off_x: " << int(glyph_offset.offset_x);
                            // SYNCSGLOG << "off_y: " << int(glyph_offset.offset_y);
                        }
                        else {
                            // Handle missing glyphs
                            // Its very unlikely that we'll get here as Font
                            // itemization should have segmented missing glyph
                            // runs out. However if harfbuzz shaping merges/changes
                            // any glyphs and the font doesn't have the required
                            // symbol (can that even occur?) we substitute in the
                            // default missing glyph.
                            GlyphInfo glyph_info;
                            glyph_info.font = 0;
                            glyph_info.index = 0;
                            glyph_info.cluster = hb_glyph_info.cluster;

                            GlyphOffset glyph_offset;
                            glyph_offset.advance_x = text_hint.glyph_res_px;
                            glyph_offset.advance_y = 0;
                            glyph_offset.offset_x  = 0;
                            glyph_offset.offset_y  = 0;

                            line.list_glyph_info.push_back(glyph_info);
                            line.list_glyph_offsets.push_back(glyph_offset);
                        }
                    }
                }

                hb_buffer_destroy(hb_buff);
            }

            // =========================================================== //

        }

        // =========================================================== //

        std::vector<TextLine>
        ShapeText(std::string const &utf8text,
                  std::vector<unique_ptr<Font>> const &list_fonts,
                  TextHint const &text_hint)
        {
            TextParagraph para;
            para.utf16text = icu::UnicodeString::fromUTF8(utf8text);
            para.utf16count = para.utf16text.length();
            para.codepoint_count = para.utf16text.countChar32();

            if(text_hint.direction != TextHint::Direction::Multiple &&
               text_hint.script == TextHint::Script::Single)
            {
                // TODO: Short cut
                // hb_buffer_set_direction
                // hb_buffer_guess_segment_properties (hb_buffer);
            }
            else
            {
                ItemizeDirection(para,HB_DIRECTION_INVALID);
                ItemizeScript(para);
            }

            ItemizeFont(list_fonts,text_hint,para);
            MergeRuns(para);

            // Add the initial line of text containing all
            // of the text to the paragraph
            para.list_lines.push_back(TextLine());
            para.list_lines.back().start = 0;
            para.list_lines.back().end = para.utf16count;

            // Shape the first line
            ShapeLine(list_fonts,text_hint,para,0);

            return para.list_lines;
        }

        // =========================================================== //
    }
}
