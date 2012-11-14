/*****************************************************************************\
 *  proctrack_libvirt.c - process tracking via libvirt
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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_STDINT_H
#include <stdint.h>
#endif
#if HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#include "slurm/slurm.h"
#include "slurm/slurm_errno.h"
#include "src/common/log.h"
#include "src/slurmd/slurmd/slurmd.h"

#include "src/slurmd/slurmstepd/slurmstepd_job.h"

#include "src/common/xlibvirt_read_config.h"

#include "proctrack_libvirt_translate.h"
#include "proctrack_libvirt_instantiate.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

/*
 * These variables are required by the generic plugin interface.  If they
 * are not found in the plugin, the plugin loader will ignore it.
 *
 * plugin_name - a string giving a human-readable description of the
 * plugin.  There is no maximum length, but the symbol must refer to
 * a valid string.
 *
 * plugin_type - a string suggesting the type of the plugin or its
 * applicability to a particular form of data or method of data handling.
 * If the low-level plugin API is used, the contents of this string are
 * unimportant and may be anything.  SLURM uses the higher-level plugin
 * interface which requires this string to be of the form
 *
 *	<application>/<method>
 *
 * where <application> is a description of the intended application of
 * the plugin (e.g., "jobcomp" for SLURM job completion logging) and <method>
 * is a description of how this plugin satisfies that application.  SLURM will
 * only load job completion logging plugins if the plugin_type string has a
 * prefix of "jobcomp/".
 *
 * plugin_version - an unsigned 32-bit integer giving the version number
 * of the plugin.  If major and minor revisions are desired, the major
 * version number may be multiplied by a suitable magnitude constant such
 * as 100 or 1000.  Various SLURM versions will likely require a certain
 * minimum version for their plugins as the job completion logging API
 * matures.
 */
const char plugin_name[]      = "Process tracking via libvirt";
const char plugin_type[]      = "proctrack/libvirt";
const uint32_t plugin_version = 91;

static slurm_libvirt_conf_t slurm_libvirt_conf;

/*
 * init() is called when the plugin is loaded, before any other functions
 * are called.  Put global initialization here.
 */
extern int init (void)
{
	/* read libvirt configuration */
	if (read_slurm_libvirt_conf(&slurm_libvirt_conf))
		return SLURM_ERROR;

	return SLURM_SUCCESS;
}

extern int fini (void)
{
	free_slurm_libvirt_conf(&slurm_libvirt_conf);
	return SLURM_SUCCESS;
}

extern int slurm_container_plugin_create (slurmd_job_t *job)
{
	xlibvirt_domain_t* domain = \
		xlibvirt_instantiate_domain(job, &slurm_libvirt_conf);

	if(xlibvirt_boot_domain(domain) < 0)
		return SLURM_ERROR;

	return SLURM_SUCCESS;
}

extern int slurm_container_plugin_add (slurmd_job_t *job, pid_t pid)
{
	return SLURM_SUCCESS;
}

extern int slurm_container_plugin_signal (uint64_t id, int signal)
{
	return SLURM_SUCCESS;
}

extern int slurm_container_plugin_destroy (uint64_t id)
{
	return SLURM_SUCCESS;
}

extern uint64_t slurm_container_plugin_find(pid_t pid)
{
	return 0;
}

extern bool slurm_container_plugin_has_pid(uint64_t cont_id, pid_t pid)
{
	return false;
}

extern int slurm_container_plugin_wait(uint64_t cont_id)
{
	return SLURM_SUCCESS;
}

extern int slurm_container_plugin_get_pids(uint64_t cont_id,
					   pid_t **pids, int *npids)
{
	return SLURM_SUCCESS;
}

/* EOF */
