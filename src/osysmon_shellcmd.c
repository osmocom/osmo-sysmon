/* Simple Osmocom System Monitor (osysmon): Support for monitoring through shell commands */

/* (C) 2019 by sysmocom - s.f.m.c. GmbH <info@sysmocom.de>
 * All Rights Reserved.
 * Author: Pau Espin Pedrol <pespin@sysmocom.de>
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
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301, USA.
 */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/sysinfo.h>

#include <osmocom/vty/vty.h>
#include <osmocom/vty/command.h>

#include "osysmon.h"
#include "value_node.h"

/***********************************************************************
 * Data model
 ***********************************************************************/

struct osysmon_shellcmd {
	struct llist_head list;
	struct {
		const char *name;
		const char *cmd;
	} cfg;
};

static struct osysmon_shellcmd *osysmon_shellcmd_find(const char *name)
{
	struct osysmon_shellcmd *oc;

	llist_for_each_entry(oc, &g_oss->shellcmds, list) {
		if (!strcmp(oc->cfg.name, name))
			return oc;
	}
	return NULL;
}

static struct osysmon_shellcmd *osysmon_shellcmd_add(const char *name, const char *cmd)
{
	struct osysmon_shellcmd *oc;

	if (osysmon_shellcmd_find(name))
		return NULL;

	oc = talloc_zero(g_oss, struct osysmon_shellcmd);
	OSMO_ASSERT(oc);
	oc->cfg.name = talloc_strdup(oc, name);
	oc->cfg.cmd = talloc_strdup(oc, cmd);
	llist_add_tail(&oc->list, &g_oss->shellcmds);
	return oc;
}

static void osysmon_shellcmd_destroy(struct osysmon_shellcmd *oc)
{
	llist_del(&oc->list);
	talloc_free(oc);
}

static void osysmon_shellcmd_run(struct osysmon_shellcmd *oc, struct value_node *parent)
{
	char buf[512];
	FILE *f;
	char *p = buf;
	long offset = 0;

	f = popen(oc->cfg.cmd, "r");
	if (!f) {
		snprintf(buf, sizeof(buf), "<popen failed (%d)>", errno);
		value_node_add(parent, oc->cfg.name, buf);
		return;
	}

	while ((offset = fread(p, 1, sizeof(buf) - 1 - (p - buf), f)) != 0) {
		p += offset;
		*p = '\0';
	}

	pclose(f);

	if (buf != p) {
		if (*(p - 1) == '\n') /* Remove final new line if exists */
			*(p - 1) = '\0';
		value_node_add(parent, oc->cfg.name, buf);
	} else {
		value_node_add(parent, oc->cfg.name, "<EMPTY>");
	}
}

/***********************************************************************
 * VTY
 ***********************************************************************/

#define CMD_STR "Configure a shell command to be executed\n"
DEFUN(cfg_shellcmd, cfg_shellcmd_cmd,
	"shellcmd NAME .TEXT",
	CMD_STR "Name of this shell command snippet\n" "Command to run\n")
{
	struct osysmon_shellcmd *oc;
	char *concat = argv_concat(argv, argc, 1);
	oc = osysmon_shellcmd_add(argv[0], concat);
	talloc_free(concat);
	if (!oc) {
		vty_out(vty, "Couldn't add shell cmd, maybe it exists?%s", VTY_NEWLINE);
		return CMD_WARNING;
	}
	return CMD_SUCCESS;
}

DEFUN(cfg_no_shellcmd, cfg_no_shellcmd_cmd,
	"no shellcmd NAME",
	NO_STR CMD_STR "Name of this shell command snippet\n")
{
	struct osysmon_shellcmd *oc;
	oc = osysmon_shellcmd_find(argv[0]);
	if (!oc) {
		vty_out(vty, "Cannot find shell cmd for '%s'%s", argv[0], VTY_NEWLINE);
		return CMD_WARNING;
	}
	osysmon_shellcmd_destroy(oc);
	return CMD_SUCCESS;
}


static void osysmon_shellcmd_vty_init(void)
{
	install_element(CONFIG_NODE, &cfg_shellcmd_cmd);
	install_element(CONFIG_NODE, &cfg_no_shellcmd_cmd);
}

/***********************************************************************
 * Runtime Code
 ***********************************************************************/

/* called once on startup before config file parsing */
int osysmon_shellcmd_init()
{
	osysmon_shellcmd_vty_init();
	return 0;
}

/* called periodically */
int osysmon_shellcmd_poll(struct value_node *parent)
{
	struct value_node *vn_file;
	struct osysmon_shellcmd *oc;

	vn_file = value_node_add(parent, "shellcmd", NULL);

	llist_for_each_entry(oc, &g_oss->shellcmds, list)
		osysmon_shellcmd_run(oc, vn_file);

	return 0;
}
