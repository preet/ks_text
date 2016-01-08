#include <ks/text/KsTextFreeType.hpp>

FreeTypeError::FreeTypeError(std::string msg) :
    ks::Exception(ks::Exception::ErrorLevel::ERROR,std::move(msg))
{}

// freetype error descriptions
#undef __FTERRORS_H__
#define FT_ERRORDEF( e, v, s )  { e, s },
#define FT_ERROR_START_LIST     {
#define FT_ERROR_END_LIST       { 0, 0 } };

const FreeTypeErrorDesc FT_ErrorDesc[] =

#include FT_ERRORS_H

namespace ks
{
    namespace text
    {
        std::string GetFreeTypeError(FT_Error error)
        {
            std::string desc = "FreeType err:";
            desc += ks::ToString(FT_ErrorDesc[error].code);
            desc += std::string(": ");
            desc += ks::ToString(FT_ErrorDesc[error].message);

            return desc;
        }
    }
}
