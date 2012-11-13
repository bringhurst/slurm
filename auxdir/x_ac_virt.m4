##*****************************************************************************
#  SYNOPSIS:
#    X_AC_VIRT
#
#  DESCRIPTION:
#    Test for LIBVIRT. If found define VIRT_LIBS
##*****************************************************************************

AC_DEFUN([X_AC_VIRT],
[
   AC_CHECK_LIB([virt],
	[virConnectOpen],
	[ac_have_virt=yes])

   AC_SUBST(LIBVIRT_LIBS)
   if test "$ac_have_virt" = "yes"; then
      VIRT_LIBS="-lvirt"
      VIRT_HEADER="libvirt/libvirt.h"
   fi

   if test "$ac_have_virt" = "yes"; then
	save_LIBS="$LIBS"
	LIBS="$VIRT_LIBS $save_LIBS"
	AC_TRY_LINK([#include <${VIRT_HEADER}>],
		    [(void)virConnectOpen(0);],
		    [], [ac_have_virt="no"])
	LIBS="$save_LIBS"
	if test "$ac_have_virt" = "yes"; then
	    AC_MSG_RESULT([LIBVIRT test program built properly.])
	else
	    AC_MSG_WARN([*** LIBVIRT test program execution failed.])
	fi
   else
      AC_MSG_WARN([Can not build libvirt support without library])
      ac_have_virt="no"
   fi
])
