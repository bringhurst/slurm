/*****************************************************************************\
 *  proctrack_libvirt_translate.c - libvirt related protocol translation
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

#include "proctrack_libvirt_translate.h"

/*
 * Prototypes for internal functions
 */
xmlChar* _build_domain_xml(xlibvirt_domain_t* domain);
xmlNodePtr _build_domain_xml_os(xlibvirt_domain_os_t* os);
xmlNodePtr _build_domain_xml_device_console(
	xlibvirt_domain_device_console_t* console);
xmlNodePtr _build_domain_xml_device_interface_source(
	xlibvirt_domain_device_interface_source_t* source);
xmlNodePtr _build_domain_xml_device_interface(
	xlibvirt_domain_device_interface_t* interface);
xmlNodePtr _build_domain_xml_device_filesystem(
	xlibvirt_domain_device_filesystem_t* filesystem);
xmlNodePtr _build_domain_xml_device_pool(xlibvirt_domain_device_pool_t* pool);
xmlNodePtr _build_domain_xml_devices(xlibvirt_domain_devices_t* devices);
xmlNodePtr _build_domain_xml_elements(
	xlibvirt_domain_elements_t* elements, char* type);

int
xlibvirt_boot_domain(xlibvirt_domain_t* domain) {
        virConnectPtr conn;
	virDomainPtr dom;

        conn = virConnectOpen("lxc+unix://");
        if(conn == NULL) {
               fprintf(stderr, "Failed to open connection to lxc+unix://\n");
                return -1;
        }

	dom = virDomainCreateXML(conn, _build_domain_xml(domain), 0);
	if(!dom) {
               fprintf(stderr, "Domain creation failed.\n");
                return -1;
	}

	fprintf(stdout, "Guest domain `%s' has booted.\n", virDomainGetName(dom));

	virDomainFree(dom);
        virConnectClose(conn);

        return 1;
}

/*****************************************************************************
 * Internal functions.
 *****************************************************************************/

/*
 * This will translate a domain struct into the xml schema required by
 * libvirt.
 */
xmlChar* _build_domain_xml(xlibvirt_domain_t* domain) {
        xmlDocPtr doc = NULL;
	xmlNodePtr elements_node = NULL;
        xmlChar *xml_buf;

        LIBXML_TEST_VERSION;
        doc = xmlNewDoc(BAD_CAST "1.0");

        elements_node = _build_domain_xml_elements(domain->opts, domain->type);

        xmlDocSetRootElement(doc, elements_node);
        xmlDocDumpFormatMemory(doc, &xml_buf, 0, 1);

        xmlFreeDoc(doc);
        xmlCleanupParser();

        return xml_buf;
}

xmlNodePtr
_build_domain_xml_os(xlibvirt_domain_os_t* os) {
	xmlNodePtr os_node = xmlNewNode(NULL, BAD_CAST "os");

	xmlNewChild(os_node, NULL, BAD_CAST "type", BAD_CAST os->type);
	xmlNewChild(os_node, NULL, BAD_CAST "init", BAD_CAST os->init);

	return os_node;
}

xmlNodePtr
_build_domain_xml_device_console(xlibvirt_domain_device_console_t* console) {
	xmlNodePtr console_node = xmlNewNode(NULL, BAD_CAST "console");
	xmlNewProp(console_node, BAD_CAST "type", BAD_CAST console->type);

	return console_node;
}

xmlNodePtr
_build_domain_xml_device_interface_source(
		xlibvirt_domain_device_interface_source_t* source) {
	xmlNodePtr source_node = xmlNewNode(NULL, BAD_CAST "source");
	xmlNewProp(source_node, BAD_CAST "network", BAD_CAST source->network);

	return source_node;
}

xmlNodePtr
_build_domain_xml_device_interface(xlibvirt_domain_device_interface_t* interface) {
	xmlNodePtr interface_node = NULL;
	int num_of_sources = interface->source_count;

	interface_node = xmlNewNode(NULL, BAD_CAST "interface");
        xmlNewProp(interface_node, BAD_CAST "type", BAD_CAST interface->type);

        while(num_of_sources > 0) {
                xmlAddChild(interface_node, \
                        _build_domain_xml_device_interface_source(
                                interface->sources[num_of_sources - 1]));
                num_of_sources--;
        }

	return interface_node;
}

xmlNodePtr
_build_domain_xml_device_filesystem(xlibvirt_domain_device_filesystem_t* filesystem) {
	xmlNodePtr filesystem_node = NULL;
	xmlNodePtr source_node = NULL;
	xmlNodePtr target_node = NULL;

	filesystem_node = xmlNewNode(NULL, BAD_CAST "filesystem");
	xmlNewProp(filesystem_node, BAD_CAST "type", BAD_CAST filesystem->type);

	source_node = xmlNewNode(NULL, BAD_CAST "source");
	xmlNewProp(source_node, BAD_CAST "dir", BAD_CAST filesystem->source->dir);

	target_node = xmlNewNode(NULL, BAD_CAST "target");
	xmlNewProp(target_node, BAD_CAST "dir", BAD_CAST filesystem->target->dir);

        xmlAddChild(filesystem_node, source_node);
        xmlAddChild(filesystem_node, target_node);

	return filesystem_node;
}

xmlNodePtr
_build_domain_xml_device_pool(xlibvirt_domain_device_pool_t* pool) {
	xmlNodePtr pool_node = xmlNewNode(NULL, BAD_CAST "pool");
	xmlNewProp(pool_node, BAD_CAST "type", BAD_CAST pool->type);

	/* TODO */

	return pool_node;
}

xmlNodePtr
_build_domain_xml_devices(xlibvirt_domain_devices_t* devices) {
	int num_of_consoles = devices->console_count;
	int num_of_interfaces = devices->interface_count;
	int num_of_filesystems = devices->filesystem_count;
	int num_of_pools = devices->pool_count;

	xmlNodePtr devices_node = NULL;

	devices_node = xmlNewNode(NULL, BAD_CAST "devices");
	xmlNewChild(devices_node, NULL, \
		BAD_CAST "emulator", BAD_CAST devices->emulator);

	while(num_of_consoles > 0) {
		xmlAddChild(devices_node, \
			_build_domain_xml_device_console(
				devices->consoles[num_of_consoles - 1]));
		num_of_consoles--;
	}

	while(num_of_interfaces > 0) {
		xmlAddChild(devices_node, \
			_build_domain_xml_device_interface(
				devices->interfaces[num_of_interfaces - 1]));
		num_of_interfaces--;
	}

	while(num_of_filesystems > 0) {
		xmlAddChild(devices_node, \
			_build_domain_xml_device_filesystem(
				devices->filesystems[num_of_filesystems - 1]));
		num_of_filesystems--;
	}

	while(num_of_pools > 0) {
		xmlAddChild(devices_node, \
			_build_domain_xml_device_pool(
				devices->pools[num_of_pools - 1]));
		num_of_pools--;
	}

	return devices_node;
}

xmlNodePtr
_build_domain_xml_elements(xlibvirt_domain_elements_t* elements, char* type) {
	xmlNodePtr os_node = NULL;
	xmlNodePtr devices_node = NULL;
	xmlNodePtr elements_node = NULL;

	elements_node = xmlNewNode(NULL, BAD_CAST "domain");
        xmlNewProp(elements_node, BAD_CAST "type", BAD_CAST type);

	if(elements != NULL) {
		xmlNewChild(elements_node, NULL, \
			    BAD_CAST "name", \
			    BAD_CAST elements->name);
		xmlNewChild(elements_node, NULL, \
			    BAD_CAST "memory", \
			    BAD_CAST elements->memory);
		xmlNewChild(elements_node, NULL, \
			    BAD_CAST "vcpu", \
			    BAD_CAST elements->vcpu);
		xmlNewChild(elements_node, NULL, \
			    BAD_CAST "on_poweroff", \
			    BAD_CAST elements->on_poweroff);
		xmlNewChild(elements_node, NULL, \
			    BAD_CAST "on_reboot", \
			    BAD_CAST elements->on_reboot);
		xmlNewChild(elements_node, NULL, \
			    BAD_CAST "on_crash", \
			    BAD_CAST elements->on_crash);

		if(elements->os != NULL) {
			xmlAddChild(elements_node, \
				_build_domain_xml_os(elements->os));
		}

		if(elements->devices != NULL) {
			xmlAddChild(elements_node, \
				_build_domain_xml_devices(elements->devices));
		}
	}

	return elements_node;
}

/* EOF */
