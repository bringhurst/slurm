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

#include "proctrack_libvirt_instantiate.h"
#include "proctrack_libvirt_translate.h"

/*
 * Populate slurm libvirt domain structure with data associated with the job
 * and configuration.
 */
xlibvirt_domain_t*
xlibvirt_instantiate_domain(slurmd_job_t* job, slurm_libvirt_conf_t* conf) {

}

/* prototypes */
xlibvirt_domain_os_t* build_domain_os();
xlibvirt_domain_devices_t* build_domain_devices();
xlibvirt_domain_device_console_t* build_domain_device_console();
xlibvirt_domain_device_filesystem_t* build_domain_device_filesystem();
xlibvirt_domain_device_filesystem_source_t* build_domain_device_filesystem_source();
xlibvirt_domain_device_filesystem_target_t* build_domain_device_filesystem_target();
xlibvirt_domain_device_interface_t* build_domain_device_interface();
xlibvirt_domain_device_interface_source_t* build_domain_device_interface_source();
xlibvirt_domain_elements_t* build_domain_elements();
xlibvirt_domain_t* build_domain();


xlibvirt_domain_os_t* build_domain_os() {
	/*
	<os>
		<type>exe</type>
		<init>/bin/bash</init>
	</os>
	*/

	xlibvirt_domain_os_t* os = (xlibvirt_domain_os_t*) malloc(sizeof(xlibvirt_domain_os_t));

	os->type = "exe";
	os->init = "/bin/bash";

	return os;
}

xlibvirt_domain_devices_t* build_domain_devices() {
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

	devices->interfaces[0] = build_domain_device_interface();
	devices->filesystems[0] = build_domain_device_filesystem();
	devices->consoles[0] = build_domain_device_console();

	devices->interface_count = 1;
	devices->filesystem_count = 1;
	devices->console_count = 1;
	devices->pool_count = 0;

	return devices;
}

xlibvirt_domain_device_console_t* build_domain_device_console() {
        /*
	<console type='pty' />
        */

	xlibvirt_domain_device_console_t* console =
		(xlibvirt_domain_device_console_t*) malloc(sizeof(xlibvirt_domain_device_console_t));

	console->type = "pty";

        return console;
}

xlibvirt_domain_device_filesystem_t* build_domain_device_filesystem() {
        /*
	<filesystem type='mount'>
	...
	</filesystem>
        */

	xlibvirt_domain_device_filesystem_t* filesystem =
		(xlibvirt_domain_device_filesystem_t*) malloc(sizeof(xlibvirt_domain_device_filesystem_t));

	filesystem->type = "mount";

	filesystem->source = build_domain_device_filesystem_source();
	filesystem->target = build_domain_device_filesystem_target();

        return filesystem;
}

xlibvirt_domain_device_filesystem_source_t* build_domain_device_filesystem_source() {
        /*
	<source dir='/'/>
        */

	xlibvirt_domain_device_filesystem_source_t* source =
		(xlibvirt_domain_device_filesystem_source_t*) malloc(sizeof(xlibvirt_domain_device_filesystem_source_t));

	source->dir = "/";

        return source;
}

xlibvirt_domain_device_filesystem_target_t* build_domain_device_filesystem_target() {
        /*
	<target dir='/'/>
        */

	xlibvirt_domain_device_filesystem_target_t* target =
		(xlibvirt_domain_device_filesystem_target_t*) malloc(sizeof(xlibvirt_domain_device_filesystem_target_t));

	target->dir = "/";

        return target;
}

xlibvirt_domain_device_interface_t* build_domain_device_interface() {
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
	interface->sources[0] = build_domain_device_interface_source();

	return interface;
}

xlibvirt_domain_device_interface_source_t* build_domain_device_interface_source() {
	/*
	<source network='default'/>
	*/

	xlibvirt_domain_device_interface_source_t* source =
		(xlibvirt_domain_device_interface_source_t*) malloc(sizeof(xlibvirt_domain_device_interface_source_t));
	source->network = "default";

	return source;
}

xlibvirt_domain_elements_t* build_domain_elements() {
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

	elements->name = "domaingen_test_domain";
	elements->clock_offset = "utc";

	elements->memory = "30000";
	elements->vcpu = "1";

        elements->on_poweroff = "destroy";
        elements->on_reboot = "restart";
        elements->on_crash = "destroy";

	elements->os = build_domain_os();
	elements->devices = build_domain_devices();

	return elements;
}

xlibvirt_domain_t* build_domain() {
	/*
	<domain type='lxc'>
	...
	</domain>
	*/

	xlibvirt_domain_t* domain =
		(xlibvirt_domain_t*) malloc(sizeof(xlibvirt_domain_t));

	domain->type = "lxc";
	domain->opts = build_domain_elements();

	return domain;
}

int main(void) {
	xlibvirt_domain_t* domain;

	domain = build_domain();
	xlibvirt_boot_domain(domain);

	return 0;
}

/* EOF */
