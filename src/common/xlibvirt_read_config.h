/*****************************************************************************\
 *  xlibvirt_read_config.h - functions and declarations for reading libvirt.conf
 *****************************************************************************
 *  Copyright (C) 2012 LANS/LANL
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

#ifndef _LIBVIRT_READ_CONFIG_H
#define _LIBVIRT_READ_CONFIG_H

#if HAVE_CONFIG_H
#  include "config.h"
#if HAVE_INTTYPES_H
#  include <inttypes.h>
#else  /* !HAVE_INTTYPES_H */
#  if HAVE_STDINT_H
#    include <stdint.h>
#  endif
#endif  /* HAVE_INTTYPES_H */
#else   /* !HAVE_CONFIG_H */
#include <stdint.h>
#endif  /* HAVE_CONFIG_H */

#define XLIBVIRT_DEFAULT_RAM 30000

/* Slurm libvirt plugins configuration parameters */
typedef struct slurm_libvirt_conf {
	char *    libvirt_mountpoint;
	char *    libvirt_type;
	uint32_t  ram_space;
} slurm_libvirt_conf_t;

/*
 * read_slurm_libvirt_conf - load the Slurm libvirt configuration from the
 *      libvirt.conf  file.
 *      This function can be called more than once if so desired.
 * RET SLURM_SUCCESS if no error, otherwise an error code
 */
extern int read_slurm_libvirt_conf(slurm_libvirt_conf_t *slurm_libvirt_conf);

/*
 * free_slurm_libvirt_conf - free storage associated with the global variable
 *	slurm_libvirt_conf
 */
extern void free_slurm_libvirt_conf(slurm_libvirt_conf_t *slurm_libvirt_conf);

#endif /* !_LIBVIRT_READ_CONFIG_H */
