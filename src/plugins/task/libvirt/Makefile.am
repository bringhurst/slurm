# Makefile for task/libvirt plugin

AUTOMAKE_OPTIONS = foreign

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src/common

pkglib_LTLIBRARIES = task_libvirt.la

# libvirt task plugin.
task_libvirt_la_SOURCES = 	task_libvirt.h task_libvirt.c
task_libvirt_la_CPPFLAGS = $(HWLOC_CPPFLAGS)
task_libvirt_la_LDFLAGS = $(SO_LDFLAGS) $(PLUGIN_FLAGS) $(HWLOC_LDFLAGS) $(HWLOC_LIBS)
