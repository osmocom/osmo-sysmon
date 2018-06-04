/* Simple Osmocom System Monitor (osysmon) */

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
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301, USA.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "osysmon.h"
#include "value_node.h"

#include <osmocom/core/msgb.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/application.h>

static struct log_info log_info = {};

static int osysmon_go_parent(struct vty *vty)
{
	switch (vty->node) {
	case CTRL_CLIENT_NODE:
	case CTRL_CLIENT_GETVAR_NODE:
		return osysmon_ctrl_go_parent(vty);
	}
	return vty->node;
}

static int osysmon_is_config_node(struct vty *vty, int node)
{
	switch (node) {
	/* no non-config-nodes */
	default:
		return 1;
	}
}


static struct vty_app_info vty_info = {
	.name = "osysmon",
	.copyright =
	"Copyright (C) 2008-2018 Harald Welte\r\n"
	"License GPLv2+: GNU GPL version 2 or later <http://gnu.org/licenses/gpl-2.0.html>\r\n"
	"This is free software: you are free to change and redistribute it.\r\n"
	"There is NO WARRANTY, to the extent permitted by law.\r\n",
	.version = "0.0", //PACKAGE_VERSION
	.go_parent_cb = osysmon_go_parent,
	.is_config_node = osysmon_is_config_node,
};


static const char *config_file = "osysmon.cfg";
struct osysmon_state *g_oss;


static void print_node(struct value_node *node)
{
	if (node->value) {
		printf("%s: %s\n", node->name, node->value);
	} else {
		struct value_node *vn;
		llist_for_each_entry(vn, &node->children, list)
			print_node(vn);
	}
}

static void display_update(struct value_node *root)
{
	print_node(root);
}

static void signal_handler(int signal)
{
	fprintf(stderr, "Signal %u received", signal);

	switch(signal) {
	case SIGUSR1:
		talloc_report(g_oss, stderr);
		break;
	default:
		break;
	}
}

static void exit_help(void)
{
	printf("Usage:\n");
	printf("\tosmo-ctrl-client HOST PORT get VARIABLE\n");
	printf("\tosmo-ctrl-client HOST PORT set VARIABLE VALUE\n");
	printf("\tosmo-ctrl-client HOST PORT monitor\n");
	exit(2);
}

int main(int argc, char **argv)
{
	int rc;

	osmo_init_logging2(NULL, &log_info);

	g_oss = talloc_zero(NULL, struct osysmon_state);
	INIT_LLIST_HEAD(&g_oss->ctrl_clients);

	vty_init(&vty_info);
	osysmon_sysinfo_init();
	osysmon_ctrl_init();

	rc = vty_read_config_file(config_file, NULL);
	if (rc < 0) {
		fprintf(stderr, "Failed to parse the config file\n");
		exit(2);
	}

	signal(SIGUSR1, &signal_handler);
	signal(SIGUSR2, &signal_handler);
	osmo_init_ignore_signals();

	while (1) {
		struct value_node *root = value_node_add(g_oss, NULL, "root", NULL);
		osysmon_sysinfo_poll(root);
		osysmon_ctrl_poll(root);

		display_update(root);
		value_node_del(root);
		sleep(1);
	}

	exit(0);
}
