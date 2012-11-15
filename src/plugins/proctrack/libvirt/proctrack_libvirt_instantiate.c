/*****************************************************************************\
 *  proctrack_libvirt_instantiate.c - Slurm job to initial domain creation.
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

#include <sys/types.h>

#include "proctrack_libvirt_instantiate.h"
#include "proctrack_libvirt_translate.h"

/* Internal function prototypes */
xlibvirt_domain_os_t*
_inst_domain_os(slurmd_job_t* job, slurm_libvirt_conf_t* conf);

xlibvirt_domain_devices_t*
_inst_domain_devices(slurmd_job_t* job, slurm_libvirt_conf_t* conf);

xlibvirt_domain_device_console_t*
_inst_domain_device_console(slurmd_job_t* job, slurm_libvirt_conf_t* conf);

xlibvirt_domain_device_filesystem_t*
_inst_domain_device_filesystem(slurmd_job_t* job, slurm_libvirt_conf_t* conf);

xlibvirt_domain_elements_t*
_inst_domain_elements(slurmd_job_t* job, slurm_libvirt_conf_t* conf);

xlibvirt_domain_device_interface_t*
_inst_domain_device_interface(slurmd_job_t* job, slurm_libvirt_conf_t* conf);

xlibvirt_domain_device_filesystem_source_t*
_inst_domain_device_filesystem_source(slurmd_job_t* job, slurm_libvirt_conf_t* conf);

xlibvirt_domain_device_filesystem_target_t*
_inst_domain_device_filesystem_target(slurmd_job_t* job, slurm_libvirt_conf_t* conf);

xlibvirt_domain_device_interface_source_t*
_inst_domain_device_interface_source(slurmd_job_t* job, slurm_libvirt_conf_t* conf);

xlibvirt_domain_t*
_inst_domain(slurmd_job_t* job, slurm_libvirt_conf_t* conf);

/*
 * Populate slurm libvirt domain structure with data associated with the job
 * and configuration.
 */
xlibvirt_domain_t*
xlibvirt_instantiate_domain(slurmd_job_t* job, slurm_libvirt_conf_t* conf) {
	return _inst_domain(job, conf);
}

/****************************************************************************
 * Internal functions.
 ****************************************************************************/

xlibvirt_domain_os_t* _inst_domain_os(slurmd_job_t* job, slurm_libvirt_conf_t* conf) {
	/*
	<os>
		<type>exe</type>
		<init>/bin/bash</init>
	</os>
	*/
	uint32_t init_argc, argv_off = 0;
	char* init_cmd_buf = (char*) malloc(sizeof(char) * 4096); /* FIXME */

	xlibvirt_domain_os_t* os = (xlibvirt_domain_os_t*) malloc(sizeof(xlibvirt_domain_os_t));

	for(init_argc = job->argc; init_argc >= 0; init_argc--) {
		argv_off += sprintf(init_cmd_buf + argv_off, \
			"%s ", job->argv[init_argc]);
	}

	os->type = "exe";
	os->init = init_cmd_buf;

	return os;
}

xlibvirt_domain_devices_t* _inst_domain_devices(slurmd_job_t* job, slurm_libvirt_conf_t* conf) {
	/*
	<devices>
		<emulator>/usr/lib/libvirt/libvirt_lxc</emulator>
		...
	</devices>
	*/
	xlibvirt_domain_devices_t* devices =
		(xlibvirt_domain_devices_t*) malloc(sizeof(xlibvirt_domain_devices_t));

	devices->emulator = "/usr/lib/libvirt/libvirt_lxc";

	devices->interfaces = (xlibvirt_domain_device_interface_t**) malloc(sizeof(xlibvirt_domain_device_interface_t*));
	devices->filesystems = (xlibvirt_domain_device_filesystem_t**) malloc(sizeof(xlibvirt_domain_device_console_t*));
	devices->consoles = (xlibvirt_domain_device_console_t**) malloc(sizeof(xlibvirt_domain_device_console_t*));

	devices->interfaces[0] = _inst_domain_device_interface(job, conf);
	devices->filesystems[0] = _inst_domain_device_filesystem(job, conf);
	devices->consoles[0] = _inst_domain_device_console(job, conf);

	devices->interface_count = 1;
	devices->filesystem_count = 1;
	devices->console_count = 1;
	devices->pool_count = 0;

	return devices;
}

xlibvirt_domain_device_console_t* _inst_domain_device_console(slurmd_job_t* job, slurm_libvirt_conf_t* conf) {
        /*
	<console type='pty' />
        */
	xlibvirt_domain_device_console_t* console =
		(xlibvirt_domain_device_console_t*) malloc(sizeof(xlibvirt_domain_device_console_t));

	console->type = "pty";

        return console;
}

xlibvirt_domain_device_filesystem_t* _inst_domain_device_filesystem(slurmd_job_t* job, slurm_libvirt_conf_t* conf) {
        /*
	<filesystem type='mount'>
	...
	</filesystem>
        */
	xlibvirt_domain_device_filesystem_t* filesystem =
		(xlibvirt_domain_device_filesystem_t*) malloc(sizeof(xlibvirt_domain_device_filesystem_t));

	filesystem->type = "mount";

	filesystem->source = _inst_domain_device_filesystem_source(job, conf);
	filesystem->target = _inst_domain_device_filesystem_target(job, conf);

        return filesystem;
}

xlibvirt_domain_device_filesystem_source_t* _inst_domain_device_filesystem_source(slurmd_job_t* job, slurm_libvirt_conf_t* conf) {
        /*
	<source dir='/'/>
        */
	xlibvirt_domain_device_filesystem_source_t* source =
		(xlibvirt_domain_device_filesystem_source_t*) malloc(sizeof(xlibvirt_domain_device_filesystem_source_t));

	source->dir = "/";

        return source;
}

xlibvirt_domain_device_filesystem_target_t* _inst_domain_device_filesystem_target(slurmd_job_t* job, slurm_libvirt_conf_t* conf) {
        /*
	<target dir='/'/>
        */
	xlibvirt_domain_device_filesystem_target_t* target =
		(xlibvirt_domain_device_filesystem_target_t*) malloc(sizeof(xlibvirt_domain_device_filesystem_target_t));

	target->dir = "/";

        return target;
}

xlibvirt_domain_device_interface_t* _inst_domain_device_interface(slurmd_job_t* job, slurm_libvirt_conf_t* conf) {
	/*
	<interface type='network'>
	...
	</interface>
	*/
	xlibvirt_domain_device_interface_t* interface =
		(xlibvirt_domain_device_interface_t*) malloc(sizeof(xlibvirt_domain_device_interface_t));

	interface->type = "network";

	interface->sources = (xlibvirt_domain_device_interface_source_t**) malloc(sizeof(xlibvirt_domain_device_interface_source_t*));

	interface->source_count = 1;
	interface->sources[0] = _inst_domain_device_interface_source(job, conf);

	return interface;
}

xlibvirt_domain_device_interface_source_t* _inst_domain_device_interface_source(slurmd_job_t* job, slurm_libvirt_conf_t* conf) {
	/*
	<source network='default'/>
	*/
	xlibvirt_domain_device_interface_source_t* source =
		(xlibvirt_domain_device_interface_source_t*) malloc(sizeof(xlibvirt_domain_device_interface_source_t));
	source->network = "default";

	return source;
}

xlibvirt_domain_elements_t* _inst_domain_elements(slurmd_job_t* job, slurm_libvirt_conf_t* conf) {
	/*
	<name>vm1</name>
	<memory>30000</memory>
	<vcpu>1</vcpu>
	<clock offset='utc'/>
	<on_poweroff>destroy</on_poweroff>
	<on_reboot>restart</on_reboot>
	<on_crash>destroy</on_crash>
	...
	*/
	xlibvirt_domain_elements_t* elements =
		(xlibvirt_domain_elements_t*) malloc(sizeof(xlibvirt_domain_elements_t));


	elements->name = itoa(job->jobid);
	elements->clock_offset = "utc";

	elements->memory = "30000";
	elements->vcpu = "1";

        elements->on_poweroff = "destroy";
        elements->on_reboot = "restart";
        elements->on_crash = "destroy";

	elements->os = _inst_domain_os(job, conf);
	elements->devices = _inst_domain_devices(job, conf);

	return elements;
}

xlibvirt_domain_t* _inst_domain(slurmd_job_t* job, slurm_libvirt_conf_t* conf) {
	/*
	<domain type='lxc'>
	...
	</domain>
	*/
	xlibvirt_domain_t* domain =
		(xlibvirt_domain_t*) malloc(sizeof(xlibvirt_domain_t));

	domain->type = "lxc";
	domain->opts = _inst_domain_elements(job, conf);

	return domain;
}
