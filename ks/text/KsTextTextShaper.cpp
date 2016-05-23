/*
   Copyright (C) 2015-2016 Preet Desai (preet.desai@gmail.com)

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
            // * start,end are both indices into a utf16 string,
            //   thus they represent code units
            struct Run
            {
                Run(uint start, uint end) :
                    start(start),
                    end(end)
                {
                    // empty
                }

                uint start;
                uint end;
            };

            // FontRun, ScriptLangRun, DirectionRun
            // * run with the same font, script, direction
            struct FontRun : Run
            {
                FontRun(uint start,
                        uint end,
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
                ScriptLangRun(uint start,
                              uint end,
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
                DirectionRun(uint start,
                             uint end,
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


            // ParagraphDesc
            // * A ParagraphDesc is the container object that holds
            //   data required for shaping a single text string
            struct ParagraphDesc
            {
                icu::UnicodeString utf16text;

                // The number of UTF-16 code units. Each code point
                // is encoded with either one or two 16-bit code units
                // (utf16count >= codepoint_count)
                u32 num_codeunits;

                // The number of 'characters' (including combining
                // marks, accents, etc)
                u32 num_codepoints;

                std::vector<FontRun> list_font_runs;
                std::vector<ScriptLangRun> list_script_runs;
                std::vector<DirectionRun> list_dirn_runs;
                std::vector<TextRun> list_runs;

                unique_ptr<std::vector<ShapedLine>> list_lines;

                // list_break_data.size == codepoint_count.
                // Contains one of the following values for each code point:
                // 0 - Line break must occur
                // 1 - Line break is allowed
                // 2 - Line break is not allowed
                // 3 - Invalid; in the middle of a codepoint
                std::vector<u8> list_break_data;
            };

            // =========================================================== //

            void PrintFontRuns(std::vector<FontRun> const &list_font_runs)
            {
                std::string output;
                std::vector<FontRun>::const_iterator it;
                for(it  = list_font_runs.begin();
                    it != list_font_runs.end(); ++it)
                {
                    std::string rundata = "[" +
                            ks::ToString(it->start) + "," +
                            ks::ToString(it->end) + "," +
                            ks::ToString(it->font) + "], ";
                    output += rundata;
                }
                LOG.Info() << "FontRun:" << output;
            }

            // =========================================================== //

            void PrintBreakData(std::vector<u8> const &list_break_data)
            {
                std::string s;

                for(auto b : list_break_data)
                {
                    std::string desc;

                    if(b == LINEBREAK_ALLOWBREAK)
                    {
                        desc = "A";
                    }
                    else if(b == LINEBREAK_NOBREAK)
                    {
                        desc = "N";
                    }
                    else if(b == LINEBREAK_INSIDEACHAR)
                    {
                        desc = "I";
                    }
                    else if(b == LINEBREAK_MUSTBREAK)
                    {
                        desc = "M";
                    }
                    else
                    {
                        desc = "linebreak error";
                    }

                    s += desc;
                    s += ",";
                }

                LOG.Info() << "BreakData: " << s;
            }

            // =========================================================== //

            void ItemizeFont(std::vector<unique_ptr<Font>> const &list_fonts,
                             Hint const &text_hint,
                             ParagraphDesc &para)
            {
                // StringCharacterIterator iterates through the
                // string by codepoint not code units
                icu::StringCharacterIterator utf16_cp_it(para.utf16text);

                // Get the font index for each glyph
                std::vector<uint> list_glyph_fonts;
                list_glyph_fonts.reserve(para.num_codeunits);

                if(text_hint.font_search == Hint::FontSearch::Explicit)
                {
                    // If the FontSearch mode is Explicit, we only search
                    // the specified font and set a missing glyph if no
                    // corresponding character exists
                    sint prev_index = -1;
                    while(utf16_cp_it.hasNext())
                    {
                        sint curr_index = utf16_cp_it.getIndex();
                        sint codepoint_sz = curr_index-prev_index;
                        prev_index = curr_index;

                        // Need this to increment
                        u32 const codepoint = utf16_cp_it.next32PostInc();
                        (void)codepoint;

                        list_glyph_fonts.insert(
                                    list_glyph_fonts.end(),
                                    codepoint_sz,
                                    text_hint.list_prio_fonts[0]);
                    }
                }
                else
                {
                    // We create a copy of the fallback fonts because we might
                    // rearrange them as we search
                    auto list_fallback_fonts = text_hint.list_fallback_fonts;

                    // If the FontSearch mode is Fallback, we search through
                    // all fonts to find a match for each glyph
                    sint prev_index = -1;
                    while(utf16_cp_it.hasNext())
                    {
                        sint curr_index = utf16_cp_it.getIndex();
                        sint codepoint_sz = curr_index-prev_index;
                        prev_index = curr_index;

                        u32 const unicode = utf16_cp_it.next32PostInc();

                        FT_UInt glyph_index = 0;

                        // Check the priority fonts first
                        for(auto const idx : text_hint.list_prio_fonts)
                        {
                            auto const &font = list_fonts[idx];

                            glyph_index = FT_Get_Char_Index(font->ft_face,unicode);

                            if(glyph_index != 0)
                            {
                                list_glyph_fonts.insert(
                                            list_glyph_fonts.end(),
                                            codepoint_sz,idx);
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

                                list_glyph_fonts.insert(
                                            list_glyph_fonts.end(),
                                            codepoint_sz,idx);
                                break;
                            }
                        }

                        // There's no valid font for this glyph so just
                        // use anything thats available; it'll show up
                        // as a missing glyph anyway
                        if(glyph_index == 0)
                        {
                            if(text_hint.list_prio_fonts.empty() == false)
                            {
                                list_glyph_fonts.insert(
                                            list_glyph_fonts.end(),
                                            codepoint_sz,
                                            text_hint.list_prio_fonts[0]);
                            }
                            else
                            {
                                list_glyph_fonts.insert(
                                            list_glyph_fonts.end(),
                                            codepoint_sz,
                                            text_hint.list_fallback_fonts[0]);
                            }
                        }
                    }
                }

                // Group the text into runs with the same font
                para.list_font_runs.reserve(para.num_codeunits+1);

                // dummy to make adding runs easier
                para.list_font_runs.push_back(
                            FontRun(0,1,std::numeric_limits<uint>::max()));

                u32 codeunit_pos=0;
                std::vector<uint>::const_iterator font_it;
                for(font_it  = list_glyph_fonts.begin();
                    font_it != list_glyph_fonts.end(); ++font_it)
                {
                    if(para.list_font_runs.back().font != (*font_it))
                    {
                        para.list_font_runs.back().end = codeunit_pos;
                        para.list_font_runs.push_back(
                                    FontRun(codeunit_pos,
                                            codeunit_pos+1,
                                            (*font_it)));
                    }
                    codeunit_pos++;
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


            void ItemizeScript(ParagraphDesc &para)
            {
                // getScriptStart/End return indices for the utf16text,
                // buffer so they represent code units

                para.list_script_runs.reserve(para.utf16text.length());

                icu_extra::ScriptRun script_run(para.utf16text.getBuffer(),
                                                para.utf16text.length());

                while(script_run.next())
                {
                    ScriptLangRun run(script_run.getScriptStart(),
                                      script_run.getScriptEnd(),
                                      IcuScriptToHB(script_run.getScriptCode()));

                    para.list_script_runs.push_back(run);
                }
            }

            // =========================================================== //

            void ItemizeDirection(ParagraphDesc &para,
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

            void MergeRuns(ParagraphDesc &para)
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

                para.list_runs.reserve(para.num_codepoints); // worst case

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
                           Hint const &text_hint,
                           ParagraphDesc &para,
                           u32 const line_idx)
            {
                (void)text_hint;

                hb_buffer_t * hb_buff = hb_buffer_create();
                ShapedLine &line = (*(para.list_lines))[line_idx];
                line.list_glyph_info.clear();
                line.list_glyph_offsets.clear();

                auto const utf16buff = para.utf16text.getBuffer();

                // direction mapper
                std::array<u8,10> lkup_hb_direction{
                    0,0,0,0,0,0,0,0,0,0
                };
                lkup_hb_direction[HB_DIRECTION_RTL]=1;

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

                    uint const glyph_count =
                            hb_buffer_get_length(hb_buff);

                    hb_glyph_info_t * hb_list_glyph_info =
                            hb_buffer_get_glyph_infos(hb_buff,NULL);

                    hb_glyph_position_t * hb_list_glyph_pos =
                            hb_buffer_get_glyph_positions(hb_buff,NULL);

                    line.list_glyph_info.reserve(glyph_count);
                    line.list_glyph_offsets.reserve(glyph_count);

                    // save glyph info and offsets
                    for(uint i=0; i < glyph_count; i++)
                    {
                        hb_glyph_info_t const &hb_glyph_info =
                                hb_list_glyph_info[i];

                        hb_glyph_position_t const &hb_glyph_pos =
                                hb_list_glyph_pos[i];

                        GlyphInfo glyph_info;
                        glyph_info.index = hb_glyph_info.codepoint;
                        glyph_info.cluster = hb_glyph_info.cluster;
                        glyph_info.font = run_it->font;
                        glyph_info.rtl = lkup_hb_direction[run_it->dirn];


                        // We special case whitespace line breaking characters
                        // (0x09 to 0x0D), which include CR, LF, FF, h and v tab
                        // and set them to zero-width so they aren't shown.

                        // This also makes it straight forward to skip them when
                        // moving through text with a cursor.

                        // (the cluster is an offset into the buffer)

                        GlyphOffset glyph_offset;

                        if(utf16buff[hb_glyph_info.cluster] >= 9 &&
                           utf16buff[hb_glyph_info.cluster] <= 13)
                        {
                           glyph_info.zero_width = true;
                           glyph_offset.advance_x = 0;
                           glyph_offset.advance_y = hb_glyph_pos.y_advance/64;
                           glyph_offset.offset_x  = 0;
                           glyph_offset.offset_y  = hb_glyph_pos.y_offset/64;

                        }
                        else
                        {
                            glyph_info.zero_width = false;
                            glyph_offset.advance_x = hb_glyph_pos.x_advance/64;
                            glyph_offset.advance_y = hb_glyph_pos.y_advance/64;
                            glyph_offset.offset_x  = hb_glyph_pos.x_offset/64;
                            glyph_offset.offset_y  = hb_glyph_pos.y_offset/64;
                        }

                        line.list_glyph_info.push_back(glyph_info);
                        line.list_glyph_offsets.push_back(glyph_offset);
                    }
                }

                hb_buffer_destroy(hb_buff);
            }

            // =========================================================== //

            void FindLineBreaks(ParagraphDesc& para)
            {
                static bool init_libunibreak = false;
                if(!init_libunibreak)
                {
                    init_linebreak();
                    init_libunibreak = true;
                }

                // We pass utf16 string data to Harfbuzz so
                // cluster indices are utf16 string indices.

                // We want the linebreaks to match up so we
                // use utf16 here as well.
                utf16_t const * utf16text_data =
                        reinterpret_cast<utf16_t const *>(
                            para.utf16text.getBuffer());

                char const * lang = ""; // default to no language
                auto const num_cu = para.num_codeunits;
                char linebreaks[num_cu];
                set_linebreaks_utf16(utf16text_data,
                                     num_cu,
                                     lang,
                                     linebreaks);
                // save line breaks
                para.list_break_data.reserve(num_cu);
                for(uint i=0; i < num_cu; i++)
                {
                    para.list_break_data.push_back(linebreaks[i]);
                }

                // Unicode's breaking rules specify that you
                // must always break at the end of text (see LB3):
                // http://unicode.org/reports/tr14/#BreakingRules

                // Since we create a new line every time a mandatory
                // break is encountered, set the last character to
                // be a NOBREAK instead iff the last character isn't
                // a LF or a CR
                if(para.list_break_data[num_cu-1] == LINEBREAK_MUSTBREAK)
                {
                    if(para.num_codeunits > 1)
                    {
                        if(para.list_break_data[num_cu-2] != LINEBREAK_INSIDEACHAR)
                        {
                            auto const utf16char = utf16text_data[num_cu-1];

                            // LF: 10, CR: 13
                            bool is_newline = (utf16char == 10 || utf16char == 13);

                            if(is_newline == false)
                            {
                                para.list_break_data[num_cu-1] = LINEBREAK_NOBREAK;
                            }
                        }
                    }
                    else
                    {
                        auto const utf16char = utf16text_data[num_cu-1];

                        // LF: 10, CR: 13
                        bool is_newline = (utf16char == 10 || utf16char == 13);

                        if(is_newline == false)
                        {
                            para.list_break_data[num_cu-1] = LINEBREAK_NOBREAK;
                        }
                    }
                }
            }

            // =========================================================== //

            void CreateNewLine(ParagraphDesc& para,
                               uint line_index,
                               uint break_index)
            {
                // Break
                ShapedLine& line = (*(para.list_lines))[line_index];

                ShapedLine line_next;
                line_next.start = break_index+1;
                line_next.end = line.end;
                line.end = line_next.start;
                para.list_lines->push_back(line_next);
            }

            void SplitIntoNewLine(ParagraphDesc& para,
                                  uint line_index,
                                  uint break_index)
            {
                // Create a new line
                para.list_lines->emplace_back();

                ShapedLine& old_line = (*(para.list_lines))[line_index];
                ShapedLine& new_line = para.list_lines->back();
                new_line.start = break_index+1;
                new_line.end = old_line.end;
                old_line.end = new_line.start;

                // Estimate number of glyphs in next line
                // TODO verify
                uint const next_line_glyph_count =
                        new_line.end-new_line.start;

                std::vector<sint> list_rem_glyphs;
                list_rem_glyphs.reserve(next_line_glyph_count);

                new_line.list_glyph_info.reserve(
                            next_line_glyph_count);

                new_line.list_glyph_offsets.reserve(
                            next_line_glyph_count);

                // Move all glyphs that belong in the next line
                for(uint i=0; i < old_line.list_glyph_info.size(); i++)
                {
                    if(old_line.list_glyph_info[i].cluster > break_index)
                    {
                        new_line.list_glyph_info.push_back(
                                    old_line.list_glyph_info[i]);

                        new_line.list_glyph_offsets.push_back(
                                    old_line.list_glyph_offsets[i]);

                        list_rem_glyphs.push_back(i);
                    }
                }

                if(list_rem_glyphs.size()==0)
                {
                    return;
                }

                // Remove glyphs from old_line
                auto old_glyph_info_begin = old_line.list_glyph_info.begin();
                auto old_glyph_offsets_begin = old_line.list_glyph_offsets.begin();

                for(sint i=list_rem_glyphs.size()-1; i >=0; i--)
                {
                    auto idx = list_rem_glyphs[i];

                    old_line.list_glyph_info.erase(
                                std::next(old_glyph_info_begin,idx));

                    old_line.list_glyph_offsets.erase(
                                std::next(old_glyph_offsets_begin,idx));
                }
            }
        }

        // =========================================================== //

        std::u16string ConvertStringUTF8ToUTF16(std::string const &utf8text)
        {
            icu::UnicodeString icu_string = icu::UnicodeString::fromUTF8(utf8text);
            return std::u16string(
                        reinterpret_cast<const char16_t*>(icu_string.getBuffer()),
                        icu_string.length());
        }

        std::string ConvertStringUTF16ToUTF8(std::u16string const &utf16text)
        {
            UChar const * data = reinterpret_cast<UChar const *>(utf16text.data());
            icu::UnicodeString icu_string = icu::UnicodeString(data,utf16text.size());
            std::string utf8text;
            icu_string.toUTF8String(utf8text);

            return utf8text;
        }

        std::string ConvertStringUTF32ToUTF8(std::u32string const &utf32text)
        {
            // This cast should be okay because the max unicode
            // code point is 1,114,112, far below max<int32_t>
            UChar32 const * data = reinterpret_cast<UChar32 const *>(utf32text.data());

            icu::UnicodeString icu_string =
                    icu::UnicodeString::fromUTF32(data,utf32text.size());

            std::string utf8text;
            icu_string.toUTF8String(utf8text);

            return utf8text;
        }

        // =========================================================== //

        unique_ptr<std::vector<ShapedLine>>
        ShapeText(std::u16string const &utf16text,
                  std::vector<unique_ptr<Font>> const &list_fonts,
                  Hint const &text_hint)
        {
            icu::UnicodeString icu_string(
                        true,
                        reinterpret_cast<const UChar *>(utf16text.data()),
                        utf16text.size());

            ParagraphDesc para;
            para.utf16text.fastCopyFrom(icu_string);
            para.num_codeunits = para.utf16text.length();
            para.num_codepoints = para.utf16text.countChar32();
            para.list_lines = make_unique<std::vector<ShapedLine>>();

//            if(text_hint.direction != Hint::Direction::Multiple &&
//               text_hint.script == Hint::Script::Single)
//            {
                // TODO: Short cut
                // hb_buffer_set_direction
                // hb_buffer_guess_segment_properties (hb_buffer);
//            }
//            else
//            {
                ItemizeDirection(para,HB_DIRECTION_INVALID);
                ItemizeScript(para);
//            }

            ItemizeFont(list_fonts,text_hint,para);
            MergeRuns(para);

            // Add the initial line of text containing all
            // of the text to the paragraph
            para.list_lines->push_back(ShapedLine());
            para.list_lines->back().start = 0;
            para.list_lines->back().end = para.num_codeunits;

            // Shape the first line
            ShapeLine(list_fonts,text_hint,para,0);

            if(text_hint.elide)
            {
                // Check if we can return early
                if(text_hint.max_line_width_px ==
                        std::numeric_limits<uint>::max())
                {
                    return std::move(para.list_lines);
                }

                ShapedLine& line = para.list_lines->back();
                uint combined_adv=0;

                // For each glyph
                for(uint i=0; i < line.list_glyph_info.size(); i++)
                {
                    combined_adv += line.list_glyph_offsets[i].advance_x;
                    if(combined_adv >= text_hint.max_line_width_px)
                    {
                        // See how much space we need for the set
                        // of elide characters '...'
                        Hint elide_text_hint = text_hint;
                        elide_text_hint.list_prio_fonts.clear();
                        elide_text_hint.list_fallback_fonts.clear();
                        elide_text_hint.list_prio_fonts.push_back(
                                    line.list_glyph_info[i].font);

                        elide_text_hint.font_search =
                                Hint::FontSearch::Explicit;

                        elide_text_hint.max_line_width_px =
                                std::numeric_limits<uint>::max();

                        elide_text_hint.elide = false;

                        auto elide_list_lines_ptr =
                                ShapeText(
                                    ConvertStringUTF8ToUTF16("..."),
                                    list_fonts,
                                    elide_text_hint);

                        uint elide_glyphs_adv=0;
                        ShapedLine& elide_line = elide_list_lines_ptr->front();
                        for(auto& elide_glyph : elide_line.list_glyph_offsets)
                        {
                            elide_glyphs_adv += elide_glyph.advance_x;
                        }

                        // Start removing glyphs until there's enough
                        // space to add the '...'
                        uint elide_space = elide_glyphs_adv; // times some k factor
                        bool space_avail = false;

                        for(sint j=i; j >= 0; j--)
                        {
                            combined_adv -= line.list_glyph_offsets[j].advance_x;
                            if((text_hint.max_line_width_px - combined_adv) > elide_space)
                            {
                                space_avail = true;

                                // Remove glyphs >= index j
                                auto it_info_e0 = std::next(line.list_glyph_info.begin(),j);
                                auto it_info_e1 = line.list_glyph_info.end();
                                line.list_glyph_info.erase(it_info_e0,it_info_e1);

                                auto it_offsets_e0 = std::next(line.list_glyph_offsets.begin(),j);
                                auto it_offsets_e1 = line.list_glyph_offsets.end();
                                line.list_glyph_offsets.erase(it_offsets_e0,it_offsets_e1);

                                // Set new line ending
                                line.end = line.list_glyph_info.back().cluster;

                                break;
                            }
                        }

                        if(!space_avail)
                        {
                            // Remove all glyphs as there isn't any space
                            // to show anything
                            line.start = 0;
                            line.end = 0;

                            line.list_glyph_info.clear();
                            line.list_glyph_offsets.clear();
                        }
                        else
                        {
                            // Manually add the '...' glyphs
                            line.list_glyph_info.insert(
                                        line.list_glyph_info.end(),
                                        elide_line.list_glyph_info.begin(),
                                        elide_line.list_glyph_info.end());

                            line.list_glyph_offsets.insert(
                                        line.list_glyph_offsets.end(),
                                        elide_line.list_glyph_offsets.begin(),
                                        elide_line.list_glyph_offsets.end());
                        }

                        break;
                    }
                }
            }
            else
            {
                // Breaking strategy from:
                // https://lists.freedesktop.org/archives/harfbuzz/2014-February/004136.html

                // Find all line breaks in the text
                FindLineBreaks(para);

                // Map cluster advances to individual code units
                // because we go through each utf16 index to check
                // for line breaks (ie codeunits, not glyphs)
                std::vector<s32> list_codeunit_adv(para.num_codeunits,0);

                {
                    std::vector<GlyphInfo> const &ls_glyph_info =
                            para.list_lines->back().list_glyph_info;

                    std::vector<GlyphOffset> const &ls_glyph_offsets =
                            para.list_lines->back().list_glyph_offsets;

                    for(size_t i=0; i < ls_glyph_info.size(); i++)
                    {
                        list_codeunit_adv[ls_glyph_info[i].cluster] +=
                                ls_glyph_offsets[i].advance_x;
                    }
                }

                // Continually split the text into lines until all
                // lines are below the max_width (or breaking is
                // no longer possible)

                uint lk_break_cu=0;

                for(uint i = 0; i < para.list_lines->size(); i++)
                {
                    ShapedLine& line = para.list_lines->back();

                    uint combined_adv = 0;

                    // For each codeunit in the line
                    for(uint cu = line.start; cu < line.end; cu++)
                    {
                        // Check if we have to break (newline, etc)
                        if(para.list_break_data[cu] == LINEBREAK_MUSTBREAK)
                        {
                            SplitIntoNewLine(para,i,cu);
                            ShapeLine(list_fonts,text_hint,para,para.list_lines->size()-2);
                            break;
                        }
                        else if(para.list_break_data[cu] == LINEBREAK_ALLOWBREAK)
                        {
                            lk_break_cu = cu;
                        }

                        combined_adv += list_codeunit_adv[cu];

                        if(combined_adv > text_hint.max_line_width_px)
                        {
                            if(lk_break_cu > line.start)
                            {
                                SplitIntoNewLine(para,i,lk_break_cu);
                                ShapeLine(list_fonts,text_hint,para,para.list_lines->size()-2);
                                break;
                            }
                        }
                    }
                }
            }

            if(para.list_dirn_runs[0].dirn == HB_DIRECTION_LTR)
            {
                auto& list_lines = *para.list_lines;
                for(auto& line : list_lines)
                {
                    line.rtl = false;
                }
            }
            else
            {
                auto& list_lines = *para.list_lines;
                for(auto& line : list_lines)
                {
                    line.rtl = true;
                }
            }

            return std::move(para.list_lines);
        }

        // =========================================================== //
    }
}
