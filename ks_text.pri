INCLUDEPATH += $${PWD}

# ks
PATH_KS_TEXT = $${PWD}/ks/text
INCLUDEPATH += $${PATH_KS_TEXT}/thirdparty

HEADERS += \
    $${PATH_KS_TEXT}/KsTextDataTypes.hpp \
    $${PATH_KS_TEXT}/KsTextFreeType.hpp \
    $${PATH_KS_TEXT}/KsTextFont.hpp \
    $${PATH_KS_TEXT}/KsTextTextAtlas.hpp \
    $${PATH_KS_TEXT}/KsTextTextShaper.hpp \
    $${PATH_KS_TEXT}/KsTextTextManager.hpp

SOURCES += \
    $${PATH_KS_TEXT}/KsTextFreeType.cpp \
    $${PATH_KS_TEXT}/KsTextTextAtlas.cpp \
    $${PATH_KS_TEXT}/KsTextTextShaper.cpp \
    $${PATH_KS_TEXT}/KsTextTextManager.cpp

# thirdparty
include($${PATH_KS_TEXT}/thirdparty/freetype/libfreetype.pri)
include($${PATH_KS_TEXT}/thirdparty/icu/libicu.pri)
include($${PATH_KS_TEXT}/thirdparty/harfbuzz/libharfbuzz.pri)
include($${PATH_KS_TEXT}/thirdparty/unibreak/libunibreak.pri)
