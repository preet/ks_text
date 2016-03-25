# icu 5.3.1
# icu build for doing bidi and script itemization
# TODO update to 5.4

# icu doesn't use "#include <...>" instead
# of "#include "..."" to differentiate
# between local and system headers, so some
# of the headers point to the system unicode
# headers instead of local ones

# try and fix:
PATH_ICU_COMMON = $${PWD}/common
QMAKE_CXXFLAGS += -iquote$${PATH_ICU_COMMON}

# defines
# based on:
# icu/readme.html#HowToBuild
DEFINES += "U_USING_ICU_NAMESPACE=0"

# force default charset to utf-8 for
# some optimization and space saving
DEFINES += "U_CHARSET_IS_UTF8=1"

# use explicit to prevent construction with
# unintended params
# [breaks ICU test suites]
DEFINES += "UNISTR_FROM_CHAR_EXPLICIT=explicit"
DEFINES += "UNISTR_FROM_STRING_EXPLICIT=explicit"

# prevent unnecessary inclusion of legacy headers
# [breaks UCI test suites]
DEFINES += "U_NO_DEFAULT_INCLUDE_UTF_HEADERS=1"

# tell libicu which modules are being built
DEFINES += U_COMMON_IMPLEMENTATION

# build type so libicu knows how to export symbols
DEFINES += U_STATIC_IMPLEMENTATION

# additional flag from Android.mk reference;
# bionic doesn't have <langinfo.h>
DEFINES += "U_HAVE_NL_LANGINFO_CODESET=0"

# turn off modules we dont use
# http://userguide.icu-project.org/packaging
# and more from uconfig.h
DEFINES += UCONFIG_NO_LEGACY_CONVERSION
DEFINES += UCONFIG_NO_NORMALIZATION # (turns off break itr, collation and translitr)
#DEFINES += UCONFIG_NO_BREAK_ITERATION
#DEFINES += UCONFIG_NO_COLLATION
#DEFINES += UCONFIG_NO_TRANSLITERATION
DEFINES += UCONFIG_NO_FORMATTING
DEFINES += UCONFIG_NO_REGULAR_EXPRESSIONS

# from uconfig.h
DEFINES += "U_ENABLE_DYLOAD=0"
DEFINES += "U_CHECK_DYLOAD=0"

# since we only care about ubidi and ubidi
# data is compiled into the lib we shouldn't
# need file IO (not sure though)
DEFINES += "UCONFIG_NO_FILE_IO=1"

#////////////////////////////////////////////////////////////////////

# common
INCLUDEPATH += $${PATH_ICU_COMMON}

# platform
SOURCES += \
    $${PATH_ICU_COMMON}/cmemory.c \
    $${PATH_ICU_COMMON}/uobject.cpp \
    $${PATH_ICU_COMMON}/cstring.c \
    $${PATH_ICU_COMMON}/cwchar.c \
    $${PATH_ICU_COMMON}/uinvchar.c \
    $${PATH_ICU_COMMON}/ustring.cpp \
    $${PATH_ICU_COMMON}/ustrfmt.c \
    $${PATH_ICU_COMMON}/utf_impl.c \
    $${PATH_ICU_COMMON}/putil.cpp \
    $${PATH_ICU_COMMON}/ucln_cmn.c \
    $${PATH_ICU_COMMON}/udataswp.c \
    $${PATH_ICU_COMMON}/umath.c \
    $${PATH_ICU_COMMON}/umutex.cpp \
    $${PATH_ICU_COMMON}/sharedobject.cpp \
    $${PATH_ICU_COMMON}/utrace.c

# appendable
SOURCES += \
    $${PATH_ICU_COMMON}/appendable.cpp

# ustrtrns
SOURCES += \
    $${PATH_ICU_COMMON}/ustrtrns.cpp

# unistr_core
SOURCES += \
    $${PATH_ICU_COMMON}/unistr.cpp

# charstr
SOURCES += \
    $${PATH_ICU_COMMON}/charstr.cpp

# uhash
SOURCES += \
    $${PATH_ICU_COMMON}/uhash.c

# udata
SOURCES += \
    $${PATH_ICU_COMMON}/udata.cpp \
    $${PATH_ICU_COMMON}/ucmndata.c \
    $${PATH_ICU_COMMON}/udatamem.c \
    $${PATH_ICU_COMMON}/umapfile.c

# ucol_swp
SOURCES += \
    $${PATH_ICU_COMMON}/ucol_swp.cpp

# utrie2
SOURCES += \
    $${PATH_ICU_COMMON}/utrie2.cpp

# sort
SOURCES += \
    $${PATH_ICU_COMMON}/uarrsort.c

# stringenumeration
SOURCES += \
    $${PATH_ICU_COMMON}/ustrenum.cpp \
    $${PATH_ICU_COMMON}/uenum.c

# resourcebundle
SOURCES += \
    $${PATH_ICU_COMMON}/resbund.cpp \
    $${PATH_ICU_COMMON}/uresbund.cpp \
    $${PATH_ICU_COMMON}/uresdata.c \
    $${PATH_ICU_COMMON}/locavailable.cpp \
    $${PATH_ICU_COMMON}/uloc.cpp \
    $${PATH_ICU_COMMON}/uloc_tag.c \
    $${PATH_ICU_COMMON}/locid.cpp \
    $${PATH_ICU_COMMON}/locmap.c \
    $${PATH_ICU_COMMON}/wintz.c \
    $${PATH_ICU_COMMON}/locbased.cpp

# bytestrie
SOURCES += \
    $${PATH_ICU_COMMON}/bytestrie.cpp

# propname
SOURCES += \
    $${PATH_ICU_COMMON}/propname.cpp

# uscript
SOURCES += \
    $${PATH_ICU_COMMON}/uscript.c

# uchar
SOURCES += \
    $${PATH_ICU_COMMON}/uchar.c

# ubidi_props
SOURCES += \
    $${PATH_ICU_COMMON}/ubidi_props.c

# ubidi
SOURCES += \
    $${PATH_ICU_COMMON}/ubidi.c \
    $${PATH_ICU_COMMON}/ubidiln.c \
    $${PATH_ICU_COMMON}/ubidiwrt.c

# stringpiece
SOURCES += \
    $${PATH_ICU_COMMON}/stringpiece.cpp

# schriter
SOURCES += \
    $${PATH_ICU_COMMON}/schriter.cpp \
    $${PATH_ICU_COMMON}/uchriter.cpp

# chariter
SOURCES += \
    $${PATH_ICU_COMMON}/chariter.cpp

# errorcode (for logging)
SOURCES += \
    $${PATH_ICU_COMMON}/errorcode.cpp

# utypes (needed for errorcode)
SOURCES += \
    $${PATH_ICU_COMMON}/utypes.c

#////////////////////////////////////////////////////////////////////

# stubdata
PATH_ICU_STUBDATA = $${PWD}/stubdata
INCLUDEPATH += $${PATH_ICU_STUBDATA}
SOURCES += \
    $${PATH_ICU_STUBDATA}/stubdata.c

#////////////////////////////////////////////////////////////////////

# extra (scrptrun is a tool for script itemization)
PATH_ICU_EXTRA = $${PWD}/extra
INCLUDEPATH += $${PATH_ICU_EXTRA}
HEADERS += $${PATH_ICU_EXTRA}/scrptrun.h
SOURCES += $${PATH_ICU_EXTRA}/scrptrun.cpp
