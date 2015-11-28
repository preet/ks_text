# freetype 2.5.3 
# with patch commit f8555b5d8cdb76134175fc3260e06c8805ede867

# modified files:
# include/config/ftmodule.h  -- comment out unwanted modules
# include/config/ftoption.h  -- comment out zlib support

PATH_FREETYPE = $${PWD}

INCLUDEPATH += $${PATH_FREETYPE}/include

HEADERS += \
    $${PATH_FREETYPE}/include/ft2build.h

SOURCES += \
    $${PATH_FREETYPE}/src/base/ftbbox.c \
    $${PATH_FREETYPE}/src/base/ftbitmap.c \
    $${PATH_FREETYPE}/src/base/ftfstype.c \
    $${PATH_FREETYPE}/src/base/ftglyph.c \
    $${PATH_FREETYPE}/src/base/ftlcdfil.c \
    $${PATH_FREETYPE}/src/base/ftstroke.c \
    $${PATH_FREETYPE}/src/base/fttype1.c \
    $${PATH_FREETYPE}/src/base/ftxf86.c \
    $${PATH_FREETYPE}/src/base/ftbase.c \
    $${PATH_FREETYPE}/src/base/ftsystem.c \
    $${PATH_FREETYPE}/src/base/ftinit.c \
    $${PATH_FREETYPE}/src/base/ftgasp.c \
    $${PATH_FREETYPE}/src/raster/raster.c \
    $${PATH_FREETYPE}/src/sfnt/sfnt.c \
    $${PATH_FREETYPE}/src/smooth/smooth.c \
    $${PATH_FREETYPE}/src/autofit/autofit.c \
    $${PATH_FREETYPE}/src/truetype/truetype.c \
    $${PATH_FREETYPE}/src/cff/cff.c \
    $${PATH_FREETYPE}/src/psnames/psnames.c \
    $${PATH_FREETYPE}/src/pshinter/pshinter.c

#    $${PATH_FREETYPE}/src/gzip/ftgzip.c \

# I think fPIC and DPIC are already
# included for qmake by default
# QMAKE_CFLAGS += -fPIC -DPIC

# The Android freetype build has this flag
# but I don't really understand why
# DEFINES += DARWIN_NO_CARBON

#
DEFINES += FT2_BUILD_LIBRARY
