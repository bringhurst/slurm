# Makefile for proctrack/libvirt plugin

AUTOMAKE_OPTIONS = foreign

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src/common $(XML2_CFLAGS)

pkglib_LTLIBRARIES = proctrack_libvirt.la

# Process group ID process tracking plugin.
proctrack_libvirt_la_SOURCES = \
			proctrack_libvirt.c \
			proctrack_libvirt_instantiate.c \
			proctrack_libvirt_translate.c
proctrack_libvirt_la_LDFLAGS = \
			$(SO_LDFLAGS) \
			$(PLUGIN_FLAGS) \
			$(VIRT_LIBS) \
			$(XML2_LIBS)
