/* Simple command-line client against the Osmocom CTRL interface */

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "simple_ctrl.h"

#include <osmocom/core/msgb.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/application.h>

static struct log_info log_info = {};

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
	struct simple_ctrl_handle *sch;
	const char *host;
	uint16_t port;
	int rc;

	if (argc < 4)
		exit_help();

	host = argv[1];
	port = atoi(argv[2]);

	osmo_init_logging2(NULL, &log_info);

	sch = simple_ctrl_open(NULL, host, port, 1000);
	if (!sch)
		exit(1);

	if (!strcmp(argv[3], "get")) {
		char *val;
		if (argc < 5)
			exit_help();
		val = simple_ctrl_get(sch, argv[4]);
		if (!val)
			exit(2);
		printf("%s\n", val);
	} else if (!strcmp(argv[3], "set")) {
		if (argc < 6)
			exit_help();
		rc = simple_ctrl_set(sch, argv[4], argv[5]);
		if (rc < 0)
			exit(1);
	} else if (!strcmp(argv[3], "monitor")) {
		simple_ctrl_set_timeout(sch, 0);
		while (true) {
			struct msgb *msg = simple_ctrl_receive(sch);
			if (!msg)
				exit(1);
			printf("%s", (char *) msgb_l2(msg));
			msgb_free(msg);
		}
	} else
		exit_help();

	exit(0);
}
