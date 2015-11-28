#ifndef KS_TEXT_FREETYPE_INCLUDE_HPP
#define KS_TEXT_FREETYPE_INCLUDE_HPP

#include <ks/KsException.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace ks
{
    namespace text
    {
        std::string GetFreeTypeError(FT_Error error);
    }
}

struct FreeTypeErrorDesc
{
    int code;
    const char* message;
};

extern const FreeTypeErrorDesc FT_ErrorDesc[];

class FreeTypeError : public ks::Exception
{
public:
    FreeTypeError(std::string msg);
    ~FreeTypeError() = default;
};

#endif // KS_TEXT_FREETYPE_INCLUDE_HPP
