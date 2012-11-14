/*****************************************************************************\
 *  proctrack_libvirt_translate.h - libvirt related protocol translation
 *****************************************************************************
 *  Copyright (C) 2012 LANS
 *  Written by Jon Bringhurst <jonb@lanl.gov>
 *
 *  This file is part of SLURM, a resource management program.
 *  For details, see <http://www.schedmd.com/slurmdocs/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  SLURM is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  SLURM is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with SLURM; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/

#if HAVE_CONFIG_H
#   include "config.h"
#endif

#if HAVE_STDINT_H
#  include <stdint.h>
#endif
#if HAVE_INTTYPES_H
#  include <inttypes.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libvirt/libvirt.h>

#ifndef _XLIBVIRT_H_
#define _XLIBVIRT_H_

/****************************************************************************
 * Interface types
 ****************************************************************************/

/*
 * An interface source which may be included in the interface source list.
 */
typedef struct {
        char* network;
} xlibvirt_domain_device_interface_source_t;

/*
 * An interface which may be included in the device list of a domain.
 */
typedef struct {
	char* type;
	size_t source_count;
	xlibvirt_domain_device_interface_source_t** sources;
} xlibvirt_domain_device_interface_t;


/****************************************************************************
 * Filesystem types
 ****************************************************************************/

/*
 * A filesystem target.
 */
typedef struct {
        char* dir;
} xlibvirt_domain_device_filesystem_target_t;

/*
 * A filesystem source.
 */
typedef struct {
        char* dir;
} xlibvirt_domain_device_filesystem_source_t;

/*
 * A filesystem which may be included in the device list of a domain.
 */
typedef struct {
	char* type;
	xlibvirt_domain_device_filesystem_source_t* source;
	xlibvirt_domain_device_filesystem_target_t* target;
} xlibvirt_domain_device_filesystem_t;


/****************************************************************************
 * Pool types
 ****************************************************************************/

/*
 * A pool target.
 */
typedef struct {
        char* path;
} xlibvirt_domain_device_pool_target_t;

/*
 * A pool source.
 */
typedef struct {
        char* host;
        char* path;
} xlibvirt_domain_device_pool_source_t;

/*
 * A pool which may be included in the device list of a domain. An example of
 * this type may be an NFS mount.
 */
typedef struct {
	char* type;
	char* name;
	xlibvirt_domain_device_pool_source_t* source;
	xlibvirt_domain_device_pool_target_t* target;
} xlibvirt_domain_device_pool_t;


/****************************************************************************
 * Basic domain types
 ****************************************************************************/

typedef struct {
        char* type;
} xlibvirt_domain_device_console_t;

/*
 * The options which define the devices available to the domain.
 */
typedef struct {
        char* emulator;

	size_t interface_count;
	size_t filesystem_count;
	size_t pool_count;
	size_t console_count;

        xlibvirt_domain_device_interface_t** interfaces;
        xlibvirt_domain_device_filesystem_t** filesystems;
        xlibvirt_domain_device_pool_t** pools;
        xlibvirt_domain_device_console_t** consoles;
} xlibvirt_domain_devices_t;

/*
 * The options which define the 'OS' to be run in the domain. Depending on
 * the capabilities of the domain driver, the OS may be a simple executable
 * binary, an init process for a namespaced full operating system, a virtual
 * machine image, or bios level bootloader instructions. 
 */
typedef struct {
        char* type;
        char* init;
} xlibvirt_domain_os_t;

/*
 * The second level options for a domain definition.
 */
typedef struct {
        char* name;
        char* clock_offset;

        char* memory;
        char* vcpu;

        char* on_poweroff;
        char* on_reboot;
        char* on_crash;

        xlibvirt_domain_os_t* os;
        xlibvirt_domain_devices_t* devices;
} xlibvirt_domain_elements_t;

/*
 * The top level libvirt domain definition. The content of opts may vary
 * based on the type value (i.e. lxc, kvm, openvz, etc).  */
typedef struct {
        char* type;
        xlibvirt_domain_elements_t* opts;
} xlibvirt_domain_t;


/****************************************************************************
 * Function prototypes
 ****************************************************************************/

int xlibvirt_boot_domain(xlibvirt_domain_t* domain);

#endif /* _XLIBVIRT_H_ */
