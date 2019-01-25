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
#include <getopt.h>

#include "config.h"
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
	.version = PACKAGE_VERSION,
	.go_parent_cb = osysmon_go_parent,
	.is_config_node = osysmon_is_config_node,
};

struct osysmon_state *g_oss;


static void print_node(struct value_node *node, unsigned int indent)
{
	unsigned int i;

	if (node->value) {
		for (i = 0; i < indent; i++)
			fputc(' ', stdout);
		printf("%s: %s\n", node->name, node->value);
	} else {
		struct value_node *vn;
		for (i = 0; i < indent; i++)
			fputc(' ', stdout);
		printf("%s\n", node->name);
		llist_for_each_entry(vn, &node->children, list)
			print_node(vn, indent+2);
	}
}

static void display_update(struct value_node *root)
{
	print_node(root, 0);
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

static void print_usage()
{
	printf("Usage: osmo-sysmon\n");
}

static void print_help()
{
	printf("  -h --help                  This text.\n");
	printf("  -c --config-file filename  The config file to use.\n");
	printf("  -d option --debug=DRLL:DCC:DMM:DRR:DRSL:DNM  Enable debugging.\n");
	printf("  -D --daemonize             Fork the process into a background daemon.\n");
	printf("  -s --disable-color         Do not print ANSI colors in the log\n");
	printf("  -T --timestamp             Prefix every log line with a timestamp.\n");
	printf("  -e --log-level number      Set a global loglevel.\n");
	printf("  -V --version               Print the version of OsmoHLR.\n");
}

static struct {
	const char *config_file;
	bool daemonize;
} cmdline_opts = {
	.config_file = "osmo-sysmon.cfg",
	.daemonize = false,
};

static void handle_options(int argc, char **argv)
{
	while (1) {
		int option_index = 0, c;
		static struct option long_options[] = {
			{"help", 0, 0, 'h'},
			{"config-file", 1, 0, 'c'},
			{"debug", 1, 0, 'd'},
			{"daemonize", 0, 0, 'D'},
			{"disable-color", 0, 0, 's'},
			{"log-level", 1, 0, 'e'},
			{"timestamp", 0, 0, 'T'},
			{"version", 0, 0, 'V' },
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "hc:d:Dse:TV",
				long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			print_usage();
			print_help();
			exit(0);
		case 'c':
			cmdline_opts.config_file = optarg;
			break;
		case 'd':
			log_parse_category_mask(osmo_stderr_target, optarg);
			break;
		case 'D':
			cmdline_opts.daemonize = 1;
			break;
		case 's':
			log_set_use_color(osmo_stderr_target, 0);
			break;
		case 'e':
			log_set_log_level(osmo_stderr_target, atoi(optarg));
			break;
		case 'T':
			log_set_print_timestamp(osmo_stderr_target, 1);
			break;
		case 'V':
			print_version(1);
			exit(0);
			break;
		default:
			/* catch unknown options *as well as* missing arguments. */
			fprintf(stderr, "Error in command line options. Exiting.\n");
			exit(-1);
			break;
		}
	}
}

int main(int argc, char **argv)
{
	int rc;

	osmo_init_logging2(NULL, &log_info);

	g_oss = talloc_zero(NULL, struct osysmon_state);
	INIT_LLIST_HEAD(&g_oss->ctrl_clients);
	INIT_LLIST_HEAD(&g_oss->netdevs);
	INIT_LLIST_HEAD(&g_oss->files);

	vty_init(&vty_info);
	handle_options(argc, argv);
	osysmon_sysinfo_init();
	osysmon_ctrl_init();
	osysmon_rtnl_init();
	osysmon_file_init();

	rc = vty_read_config_file(cmdline_opts.config_file, NULL);
	if (rc < 0) {
		fprintf(stderr, "Failed to parse the config file %s\n",
			cmdline_opts.config_file);
		exit(2);
	}

	signal(SIGUSR1, &signal_handler);
	signal(SIGUSR2, &signal_handler);
	osmo_init_ignore_signals();

	if (cmdline_opts.daemonize) {
		rc = osmo_daemonize();
		if (rc < 0) {
			perror("Error during daemonize");
			exit(1);
		}
	}

	while (1) {
		struct value_node *root = value_node_add(NULL, "root", NULL);
		osysmon_sysinfo_poll(root);
		osysmon_ctrl_poll(root);
		osysmon_rtnl_poll(root);
		osysmon_file_poll(root);

		display_update(root);
		value_node_del(root);
		sleep(1);
	}

	exit(0);
}
