##*****************************************************************************
#  SYNOPSIS:
#    X_AC_LIBVIRT
#
#  DESCRIPTION:
#    Test for LIBVIRT. If found define LIBVIRT_LIBS
##*****************************************************************************

AC_DEFUN([X_AC_LIBVIRT],
[
   AC_CHECK_LIB([virt],
	[virConnectOpen],
	[ac_have_libvirt=yes])

   AC_SUBST(LIBVIRT_LIBS)
   if test "$ac_have_libvirt" = "yes"; then
      LIBVIRT_LIBS="-lvirt"
      LIBVIRT_HEADER="libvirt/libvirt.h"
   fi

   if test "$ac_have_libvirt" = "yes"; then
	save_LIBS="$LIBS"
	LIBS="$LIBVIRT_LIBS $save_LIBS"
	AC_TRY_LINK([#include <${LIBVIRT_HEADER}>],
		    [(void)virConnectOpen(0);],
		    [], [ac_have_libvirt="no"])
	LIBS="$save_LIBS"
	if test "$ac_have_libvirt" = "yes"; then
	    AC_MSG_RESULT([LIBVIRT test program built properly.])
	else
	    AC_MSG_WARN([*** LIBVIRT test program execution failed.])
	fi
   else
      AC_MSG_WARN([Can not build libvirt support without library])
      ac_have_libvirt="no"
   fi
])
