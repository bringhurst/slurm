##*****************************************************************************
#  SYNOPSIS:
#    X_AC_XML2
#
#  DESCRIPTION:
#    Test for LIBXML2. If found define XML2_LIBS
##*****************************************************************************

AC_DEFUN([X_AC_XML2],
[
    AC_CHECK_LIB([xml2],
        [xmlNewDoc],
        [ac_have_xml2=yes])

    AC_PATH_PROG(XML2_CONFIG, xml2-config, no)
    if test "$XML2_CONFIG" = "no" ; then
        ac_have_xml2=no
    fi

    AC_SUBST(XML2_LIBS)
    AC_SUBST(XML2_CFLAGS)
    if test "$ac_have_xml2" = "yes"; then
        XML2_CFLAGS=`$XML2_CONFIG --cflags`
        XML2_LIBS=`$XML2_CONFIG --libs`
    fi

    if test "$ac_have_xml2" = "yes"; then
        save_CFLAGS="$CFLAGS"
        save_LIBS="$LIBS"
        CFLAGS="$XML2_CFLAGS $save_CFLAGS"
        LIBS="$XML2_LIBS $save_LIBS"
        AC_TRY_LINK([#include <libxml/xmlversion.h>],
                    [LIBXML_TEST_VERSION; return 0;],
                    [], [ac_have_xml2="no"])
        LIBS="$save_LIBS"
        CFLAGS="$save_CFLAGS"
        if test "$ac_have_xml2" = "yes"; then
            AC_MSG_RESULT([LIBXML2 test program built properly.])
        else
            AC_MSG_WARN([*** LIBXML2 test program execution failed.])
        fi
    else
        AC_MSG_WARN([Can not build libxml2 support without library])
        ac_have_xml2="no"
    fi
])
