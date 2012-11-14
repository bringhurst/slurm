/*****************************************************************************\
 *  xlibvirt_read_config.c - functions for reading libvirt.conf
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

#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "slurm/slurm_errno.h"
#include "src/common/log.h"
#include "src/common/list.h"
#include "src/common/macros.h"
#include "src/common/parse_config.h"
#include "src/common/parse_time.h"
#include "src/common/read_config.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"

#include "xlibvirt_read_config.h"

#define DEFAULT_LIBVIRT_TYPE "lxc"
#define DEFAULT_LIBVIRT_BASEDIR "/"

slurm_libvirt_conf_t *slurm_libvirt_conf = NULL;

/* Local functions */
static void _clear_slurm_libvirt_conf(slurm_libvirt_conf_t *slurm_libvirt_conf);
static char * _get_conf_path(void);

/*
 * free_slurm_libvirt_conf - free storage associated with the global variable
 *	slurm_libvirt_conf
 */
extern void free_slurm_libvirt_conf(slurm_libvirt_conf_t *slurm_libvirt_conf)
{
	_clear_slurm_libvirt_conf(slurm_libvirt_conf);
}

static void _clear_slurm_libvirt_conf(slurm_libvirt_conf_t *slurm_libvirt_conf)
{
	if (slurm_libvirt_conf) {
		xfree(slurm_libvirt_conf->libvirt_type);
		xfree(slurm_libvirt_conf->libvirt_mountpoint);
		slurm_libvirt_conf->ram_space = XLIBVIRT_DEFAULT_RAM;
	}
}

/*
 *   Parse a floating point value in s and return in val
 *    Return -1 on error and leave *val unchanged.
 */
static int str_to_float (char *s, float *val)
{
	float f;
	char *p;

	errno = 0;
	f = strtof (s, &p);

	if ((*p != '\0') || (errno != 0))
		return (-1);

	*val = f;
	return (0);
}

static void conf_get_float (s_p_hashtbl_t *t, char *name, float *fp)
{
	char *str;
	if (!s_p_get_string(&str, name, t))
		return;
	if (str_to_float (str, fp) < 0)
		fatal ("libvirt.conf: Invalid value '%s' for %s", str, name);
}

/*
 * read_slurm_libvirt_conf - load the Slurm libvirt configuration from the
 *	libvirt.conf file.
 * RET SLURM_SUCCESS if no error, otherwise an error code
 */
extern int read_slurm_libvirt_conf(slurm_libvirt_conf_t *slurm_libvirt_conf)
{
	s_p_options_t options[] = {
		{"LibvirtType", S_P_STRING},
		{"LibvirtMountpoint", S_P_STRING},
		{"RAMSpace", S_P_UINT32},
		{NULL} };

	s_p_hashtbl_t *tbl = NULL;
	char *conf_path = NULL;
	struct stat buf;

	/* Set initial values */
	if (slurm_libvirt_conf == NULL) {
		return SLURM_ERROR;
	}
	_clear_slurm_libvirt_conf(slurm_libvirt_conf);

	/* Get the libvirt.conf path and validate the file */
	conf_path = _get_conf_path();
	if ((conf_path == NULL) || (stat(conf_path, &buf) == -1)) {
		info("No libvirt.conf file (%s)", conf_path);
	} else {
		debug("Reading libvirt.conf file %s", conf_path);

		tbl = s_p_hashtbl_create(options);
		if (s_p_parse_file(tbl, NULL, conf_path, false) ==
		    SLURM_ERROR) {
			fatal("Could not open/read/parse libvirt.conf file %s",
			      conf_path);
		}

		/* libvirt initialisation parameters */
		if (!s_p_get_string(&slurm_libvirt_conf->libvirt_mountpoint,
				"LibvirtType", tbl))
			slurm_libvirt_conf->libvirt_type =
				xstrdup(DEFAULT_LIBVIRT_TYPE);

		if (!s_p_get_string(&slurm_libvirt_conf->libvirt_mountpoint,
				"LibvirtMountpoint", tbl))
			slurm_libvirt_conf->libvirt_mountpoint =
				xstrdup(DEFAULT_LIBVIRT_BASEDIR);

		s_p_get_uint32 (&slurm_libvirt_conf->ram_space,
		                "RAMSpace", tbl);

		s_p_hashtbl_destroy(tbl);
	}

	xfree(conf_path);

	return SLURM_SUCCESS;
}

/* Return the pathname of the libvirt.conf file.
 * xfree() the value returned */
static char * _get_conf_path(void)
{
	char *val = getenv("SLURM_CONF");
	char *path = NULL;
	int i;

	if (!val)
		val = default_slurm_config_file;

	/* Replace file name on end of path */
	i = strlen(val) + 15;
	path = xmalloc(i);
	strcpy(path, val);
	val = strrchr(path, (int)'/');
	if (val)	/* absolute path */
		val++;
	else		/* not absolute path */
		val = path;
	strcpy(val, "libvirt.conf");

	return path;
}
