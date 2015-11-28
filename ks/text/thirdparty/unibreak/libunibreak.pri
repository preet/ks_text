# unibreak
# https://github.com/adah1972/libunibreak
# TODO update to the latest version

PATH_UNIBREAK = $${PWD}

INCLUDEPATH += $${PATH_UNIBREAK}

SOURCES += \
    $${PATH_UNIBREAK}/linebreak.c \
    $${PATH_UNIBREAK}/linebreakdata.c \
    $${PATH_UNIBREAK}/linebreakdef.c \
    $${PATH_UNIBREAK}/wordbreak.c \
    $${PATH_UNIBREAK}/wordbreakdata.c

