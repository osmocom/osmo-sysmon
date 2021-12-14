/* Simple Osmocom System Monitor (osysmon): Support for monitoring files */

/* (C) 2018 by Harald Welte <laforge@gnumonks.org>
 * All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0+
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <string.h>
#include <sys/sysinfo.h>

#include <osmocom/vty/vty.h>
#include <osmocom/vty/command.h>

#include "osysmon.h"
#include "value_node.h"

/***********************************************************************
 * Data model
 ***********************************************************************/

struct osysmon_file {
	struct llist_head list;
	struct {
		const char *name;
		const char *path;
	} cfg;
};

static struct osysmon_file *osysmon_file_find(const char *name)
{
	struct osysmon_file *of;

	llist_for_each_entry(of, &g_oss->files, list) {
		if (!strcmp(of->cfg.name, name))
			return of;
	}
	return NULL;
}

static struct osysmon_file *osysmon_file_add(const char *name, const char *path)
{
	struct osysmon_file *of;

	if (osysmon_file_find(name))
		return NULL;

	of = talloc_zero(g_oss, struct osysmon_file);
	OSMO_ASSERT(of);
	of->cfg.name = talloc_strdup(of, name);
	of->cfg.path = talloc_strdup(of, path);
	llist_add_tail(&of->list, &g_oss->files);
	return of;
}

static void osysmon_file_destroy(struct osysmon_file *of)
{
	llist_del(&of->list);
	talloc_free(of);
}

static void osysmon_file_read(struct osysmon_file *of, struct value_node *parent)
{
	char buf[512];
	char *s, *nl;
	FILE *f;

	f = fopen(of->cfg.path, "r");
	if (!f) {
		value_node_add(parent, of->cfg.name, "<NOTFOUND>");
		return;
	}
	s = fgets(buf, sizeof(buf), f);
	fclose(f);
	if (s == NULL) {
		value_node_add(parent, of->cfg.name, "<EMPTY>");
		return;
	}
	buf[sizeof(buf)-1] = '\0';
	while ((nl = strrchr(buf, '\n')))
		*nl = '\0';
	value_node_add(parent, of->cfg.name, buf);
}

/***********************************************************************
 * VTY
 ***********************************************************************/

#define FILE_STR "Configure a file to be monitored/watched\n"
DEFUN(cfg_file, cfg_file_cmd,
	"file NAME PATH",
	FILE_STR "Name of this file-watcher\n" "Path of file in filesystem\n")
{
	struct osysmon_file *of;
	of = osysmon_file_add(argv[0], argv[1]);
	if (!of) {
		vty_out(vty, "Couldn't add file-watcher, maybe it exists?%s", VTY_NEWLINE);
		return CMD_WARNING;
	}
	return CMD_SUCCESS;
}

DEFUN(cfg_no_file, cfg_no_file_cmd,
	"no file NAME",
	NO_STR FILE_STR "Name of this file-watcher\n")
{
	struct osysmon_file *of;
	of = osysmon_file_find(argv[0]);
	if (!of) {
		vty_out(vty, "Cannot find file-watcher for '%s'%s", argv[0], VTY_NEWLINE);
		return CMD_WARNING;
	}
	osysmon_file_destroy(of);
	return CMD_SUCCESS;
}


static void osysmon_file_vty_init(void)
{
	install_element(CONFIG_NODE, &cfg_file_cmd);
	install_element(CONFIG_NODE, &cfg_no_file_cmd);
}

/***********************************************************************
 * Runtime Code
 ***********************************************************************/

/* called once on startup before config file parsing */
int osysmon_file_init()
{
	osysmon_file_vty_init();
	return 0;
}

/* called periodically */
int osysmon_file_poll(struct value_node *parent)
{
	struct value_node *vn_file;
	struct osysmon_file *of;

	if (llist_empty(&g_oss->files))
		return 0;

	vn_file = value_node_add(parent, "file", NULL);

	llist_for_each_entry(of, &g_oss->files, list)
		osysmon_file_read(of, vn_file);

	return 0;
}
